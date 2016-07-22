
#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of images to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
ifndef PDIR
GEN_LIBS = mbedtls.a
endif

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#
#DEFINES += 

#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES += -I $(PDIR)include -I./mbedtls/include
INCLUDES += -I ./
INCLUDES += -I ../libc
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

# Component makefile for mbedtls

# mbedtls by default builds into 3 libraries not one. We just use one for now (doesn't make a huge difference when static linking)

# Config:
# We supply our own hand tweaked mbedtls/config.h in our 'include' dir, the rest of upstream mbedtls is not changed.
imbedtls_ROOT = ./app/mbedtls
MBEDTLS_DIR = $(mbedtls_ROOT)mbedtls/
INC_DIRS += $(mbedtls_ROOT)include $(MBEDTLS_DIR)include

# these OBJS_xxx variables  are copied directly from mbedtls/mbedtls/Makefile,
# minus a few values (noted in comments)
#
# If updating to a future mbedtls version you can just copy these in.
OBJS_CRYPTO=	aes.o		aesni.o		arc4.o		\
		asn1parse.o	asn1write.o	base64.o	\
		bignum.o	blowfish.o	camellia.o	\
		ccm.o		cipher.o	cipher_wrap.o	\
		ctr_drbg.o	des.o		dhm.o		\
		ecdh.o		ecdsa.o		ecp.o		\
		ecp_curves.o	entropy.o	entropy_poll.o	\
		error.o		gcm.o		havege.o	\
		hmac_drbg.o	md.o		md2.o		\
		md4.o		md5.o		md_wrap.o	\
		memory_buffer_alloc.o		oid.o		\
		padlock.o	pem.o		pk.o		\
		pk_wrap.o	pkcs12.o	pkcs5.o		\
		pkparse.o	pkwrite.o	platform.o	\
		ripemd160.o	rsa.o		sha1.o		\
		sha256.o	sha512.o	threading.o	\
		timing.o	version.o			\
		version_features.o		xtea.o
# minus net.o

OBJS_X509=	certs.o		pkcs11.o	x509.o		\
		x509_create.o	x509_crl.o	x509_crt.o	\
		x509_csr.o	x509write_crt.o	x509write_csr.o

OBJS_TLS=	debug.o				ssl_cache.o	\
		ssl_ciphersuites.o		ssl_cli.o	\
		ssl_cookie.o	ssl_srv.o	ssl_ticket.o	\
		ssl_tls.o

# args for passing into compile rule generation
mbedtls_INC_DIR = $(MBEDTLS_DIR)/mbedtls/include
mbedtls_SRC_DIR = $(mbedtls_ROOT)
mbedtls_EXTRA_SRC_FILES = $(patsubst %.o,$(MBEDTLS_DIR)library/%.c,$(OBJS_CRYPTO) $(OBJS_X509) $(OBJS_TLS))

# depending on cipher configuration, some mbedTLS variables are unused
mbedtls_CFLAGS = -Wno-error=unused-but-set-variable -Wno-error=unused-variable $(CFLAGS) 

$(eval $(call component_compile_rules,mbedtls))

# Helpful error if git submodule not initialised
$(MBEDTLS_DIR):
	$(error "mbedtls git submodule not installed. Please run 'git submodule update --init'")
