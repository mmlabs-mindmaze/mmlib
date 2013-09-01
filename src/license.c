/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _XOPEN_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>

#include "license.h"

#define HASH_ALGO	GNUTLS_DIG_SHA1

static
const unsigned char ca_inl[] = {
#include "ca-file.inl"
};


static
const gnutls_datum_t ca_data = {
	.data = (unsigned char*) ca_inl,
	.size = sizeof(ca_inl),
};


static
void load_data(gnutls_datum_t* dat, FILE* f)
{
	char line[256];
	size_t len;

	dat->data = NULL;
	dat->size = 0;

	while (1) {
		len = fread(line, 1, sizeof(line), f);
		if (len == 0)
			break;

		dat->data = realloc(dat->data, dat->size+len);
		memcpy(dat->data + dat->size, line, len);
		dat->size += len;
	}
}

static
int load_hwinfo(gnutls_datum_t* hw, const char* optfile)
{
	FILE* f;

	if (!optfile)
		f = popen("lspci -n", "r");
	else
		f = fopen(optfile, "r");

	if (f == NULL) {
		perror("Cannot load HW data");
		return -1;
	}
	load_data(hw, f);
	fclose(f);

	return 0;
}


static
int load_crt(gnutls_x509_crt_t cert, const char* certfile)
{
	int r;
	FILE* f;
	gnutls_datum_t certdat = {.data = NULL, .size = 0};
	
	f = fopen(certfile, "r");
	if (f == NULL) {
		fprintf(stderr, "Failed to load certificate '%s': %s\n",
		                certfile, strerror(errno));
		return -1;
	}
	load_data(&certdat, f);
	fclose(f);

	r = gnutls_x509_crt_import(cert, &certdat, GNUTLS_X509_FMT_PEM);
	if (r)
		fprintf(stderr, "Failed to load certificate '%s': %s\n",
		                certfile, gnutls_strerror(r));

	free(certdat.data);
	return r;
}


static
int load_privkey(gnutls_x509_privkey_t key, const char* keypath)
{
	int r;
	FILE* f;
	gnutls_datum_t keydat = {.data = NULL, .size = 0};

	f = fopen(keypath, "r");
	if (f == NULL) {
		fprintf(stderr, "Failed to load private key '%s': %s\n",
		                keypath, strerror(errno));
		return errno;
	}
	load_data(&keydat, f);
	fclose(f);

	r = gnutls_x509_privkey_import(key, &keydat, GNUTLS_X509_FMT_PEM);
	if (r)
		fprintf(stderr, "Failed to load private key '%s': %s\n",
		                keypath, gnutls_strerror(r));

	free(keydat.data);
	return r;
}

static
const char inthex[16] = {
	'0', '1', '2', '3', '4' ,'5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};


static
void bin_to_hexstr(const void* restrict data, size_t len, char* restrict str)
{
	const unsigned char* udata = data;
	size_t i;

	for (i = 0; i < len; i++) {
		str[2*i]   = inthex[udata[i]%16];
		str[2*i+1] = inthex[udata[i]/16];
	}
	str[2*i] = '\0';
}

static
void get_fingerprint_str(const gnutls_datum_t* dat, char* restrict fingerprint)
{
	int len = gnutls_hash_get_len(HASH_ALGO);
	char digest[len];

	gnutls_hash_fast(HASH_ALGO, dat->data, dat->size, &digest);
	bin_to_hexstr(digest, len, fingerprint);
}


static
int sign_hwinfo(gnutls_x509_privkey_t key, gnutls_x509_crt_t crt,
                 const gnutls_datum_t* hw, FILE* f)
{
	int r = 0;
	gnutls_datum_t sig = {.data = NULL};
	gnutls_privkey_t priv;
	gnutls_pubkey_t pub;
	gnutls_digest_algorithm_t hash = GNUTLS_DIG_NULL;

	gnutls_privkey_init(&priv);
	gnutls_pubkey_init(&pub);

	if ( (r = gnutls_privkey_import_x509(priv, key, 0))
	  || (r = gnutls_pubkey_import_x509(pub, crt, 0)) ) {
		fprintf(stderr, "Failed to import data: %s\n",
		                gnutls_strerror(r));
		goto exit;
	}

	gnutls_pubkey_get_preferred_hash_algorithm(pub, &hash, NULL);
	gnutls_privkey_sign_data(priv, hash, 0, hw, &sig);
	if ( fwrite(&sig.size, sizeof(sig.size), 1, f) < 1
	  || fwrite(sig.data, sig.size, 1, f) < 1) {
		r = errno;
		fprintf(stderr, "Failed to write signature : %s\n",
		                strerror(errno));
		goto exit;
	}

exit:
	gnutls_privkey_deinit(priv);
	gnutls_pubkey_deinit(pub);
	gnutls_free(sig.data);
	return r;
}


static
int write_issuer(gnutls_x509_crt_t crt, FILE* f)
{
	int r = GNUTLS_E_SHORT_MEMORY_BUFFER;
	size_t bsize = 2048;
	gnutls_datum_t crtdat;


	while (r == GNUTLS_E_SHORT_MEMORY_BUFFER) {
		unsigned char buffer[bsize];
		r = gnutls_x509_crt_export(crt, GNUTLS_X509_FMT_DER,
		                           buffer, &bsize);
		if (r)
			continue;
		
		crtdat.data = buffer;
		crtdat.size = bsize;
		if ( fwrite(&crtdat.size, sizeof(crtdat.size), 1, f) < 1
		  || fwrite(crtdat.data, crtdat.size, 1, f) < 1) {
			fprintf(stderr, "Failed to write signature : %s\n",
		                strerror(errno));
			return errno;
		}
	}

	return 0;
}


static
int write_signature(gnutls_x509_privkey_t key, gnutls_x509_crt_t crt,
                     const gnutls_datum_t* hw, const char* path)
{
	char fingerprint[2*gnutls_hash_get_len(HASH_ALGO)+1];
	char tmp[256];
	FILE* f;
	int r;

	get_fingerprint_str(hw, fingerprint);
	sprintf(tmp, "%s/%s.sig", path, fingerprint);
	f = fopen(tmp, "w");
	if (f == NULL) {
		fprintf(stderr, "Failed to open signature file '%s': %s\n",
		                tmp, strerror(errno));
		return errno;
	}

	r = write_issuer(crt, f);
	if (!r)
		sign_hwinfo(key, crt, hw, f);

	fclose(f);
	return r;
}


static
int read_issuer(gnutls_x509_crt_t crt, FILE* f)
{
	gnutls_datum_t crtdat = {.data = NULL};
	int r;

	if (fread(&crtdat.size, sizeof(crtdat.size), 1, f) < 1)
		return errno;

	crtdat.data = malloc(crtdat.size);
	if (!crtdat.data)
		return -1;

	if (fread(crtdat.data, crtdat.size, 1, f) < 1)
		return errno;
	
	r = gnutls_x509_crt_import(crt, &crtdat, GNUTLS_X509_FMT_DER);

	return r;
}


static
int check_hwinfo_sig(gnutls_x509_crt_t crt,
                      const gnutls_datum_t* hw, FILE* f)
{
	int r;
	gnutls_datum_t sig = {.data = NULL};
	gnutls_pubkey_t pub;
	gnutls_digest_algorithm_t hash;

	gnutls_pubkey_init(&pub);
	gnutls_pubkey_import_x509(pub, crt, 0);

	fread(&sig.size, sizeof(sig.size), 1, f);
	sig.data = malloc(sig.size);
	if (!sig.data)
		return -1;

	fread(sig.data, sig.size, 1, f);

	gnutls_pubkey_get_verify_algorithm(pub, &sig, &hash);
	r = gnutls_pubkey_verify_data(pub, hash, hw, &sig);
	if (r < 0) gnutls_perror(r);

	free(sig.data);
	gnutls_pubkey_deinit(pub);

	return (r >= 0) ? 0 : -1;
}


static
int read_signature(gnutls_x509_crt_t crt, const gnutls_datum_t* hw,
                   const char* path)
{
	char fingerprint[2*gnutls_hash_get_len(HASH_ALGO)+1];
	char tmp[256];
	FILE* f;
	int r;

	get_fingerprint_str(hw, fingerprint);
	sprintf(tmp, "%s/mindmaze/%s.sig", path, fingerprint);
	f = fopen(tmp, "r");
	if (f == NULL)
		return errno;

	read_issuer(crt, f);
	r = check_hwinfo_sig(crt, hw, f);

	fclose(f);
	return r;
}


static
int read_control_sig(gnutls_x509_crt_t crt, const gnutls_datum_t* hw,
                     const gnutls_x509_crt_t ca, const char* path)
{
	int r;
	unsigned int status = 0;

	if ((r = read_signature(crt, hw, path)))
		return r;

	if ((r = gnutls_x509_crt_verify(crt, &ca, 1, 0, &status)))
		return r;

	return (status & GNUTLS_CERT_INVALID) ?
	         GNUTLS_E_X509_CERTIFICATE_ERROR : 0;
}


LOCAL_SYMBOL
int check_signature(const char* hwfile)
{
	int r = 0;
	gnutls_datum_t hw;
	gnutls_x509_crt_t crt, ca;
	char tmp[128];
	const char* homecfg = getenv("XDG_CONFIG_HOME");
	const char* altdir = getenv("MM_LIC_ALTDIR");

	if (!homecfg) {
		sprintf(tmp, "%s/.config", getenv("HOME"));
		homecfg = tmp;
	}

	gnutls_x509_crt_init(&crt);

	load_hwinfo(&hw, hwfile);
	gnutls_x509_crt_init(&ca);
	gnutls_x509_crt_import(ca, &ca_data, GNUTLS_X509_FMT_DER);

	r = read_control_sig(crt, &hw, ca, SYSCONFDIR);
	if (r)
		r = read_control_sig(crt, &hw, ca, homecfg);
	if (r && altdir)
		r = read_control_sig(crt, &hw, ca, altdir);

	gnutls_x509_crt_deinit(ca);
	gnutls_x509_crt_deinit(crt);
	gnutls_global_deinit();

	free(hw.data);

	return r;
}


LOCAL_SYMBOL
int gen_signature(const char* hwfile, const char* sigpath,
	             const char* pubkey, const char* privkey)
{
	int r = 0;
	gnutls_datum_t hw;
	gnutls_x509_crt_t crt;
	gnutls_x509_privkey_t key;

	gnutls_x509_crt_init(&crt);
	gnutls_x509_privkey_init(&key);

	if ( load_hwinfo(&hw, hwfile)
	  || load_crt(crt, pubkey)
	  || load_privkey(key, privkey)
	  || write_signature(key, crt, &hw, sigpath) )
		r = -1;

	gnutls_x509_privkey_deinit(key);
	gnutls_x509_crt_deinit(crt);

	free(hw.data);
	return r;
}

