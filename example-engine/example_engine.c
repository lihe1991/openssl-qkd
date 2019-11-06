#include <stdbool.h>
#include <openssl/types.h>
#include <openssl/engine.h>
#include <openssl/dh.h>
#include "qkd_api.h"

/* We can control a "fixed" key (instead of an actual QKD-negotiated key) to allow end-to-end
   testing before the interaction with the QKD-API has actually been implemented. */
static bool return_fixed_key_for_testing = true;

/* When we run on SimulaQron, we do certain things differently than "in real life" (see below) */
static bool running_on_simulaqron = false;

/* In real life we need 2log(P) bits of shared secret, where P is the prime number parameter
   of Diffie-Hellman. The shared secret is generated by asking QKD for a key using the ETSI API.
   In real life, we typically need 2048 bits = 256 bytes. However, generating 2048 bits of key
   material would take waaaay too long in simulation, so if we are running on top of SimulaQron
   we actually ask for fewer bits of key material. */

int shared_secret_nr_bits(DH *dh)
{
    if (running_on_simulaqron) {
        return 64;
    } else  {
        return 64;  /* TODO */
    }

    // NOTES
    // if (BN_num_bits(dh->p) > OPENSSL_DH_MAX_MODULUS_BITS) {
    //     DHerr(DH_F_GENERATE_KEY, DH_R_MODULUS_TOO_LARGE);
    //     return 0;
    // }        
    //     BIGNUM *p = BN_new();
    //     if (NULL == p) {
    //         /* CONTINUE FROM HERE */
    //     }
    //     DH_get0_pqg(&p);
    //     /* ... */

    //     long private_key_length_in_bits = DH_get_length(dh);
    //     /* TODO */
    // }
}

static const char *example_engine_id = "example";

static const char *example_engine_name = "Example Engine by Bruno Rijsman";

/* The DH_METHOD struct definition is not provided in a public OpenSSL header? Really? */
struct dh_method {
    char *name;
    int (*generate_key) (DH *dh);
    int (*compute_key) (unsigned char *key, const BIGNUM *pub_key, DH *dh);
    int (*bn_mod_exp) (const DH *dh, BIGNUM *r, const BIGNUM *a,
                       const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                       BN_MONT_CTX *m_ctx);
    int (*init) (DH *dh);
    int (*finish) (DH *dh);
    int flags;
    char *app_data;
    int (*generate_params) (DH *dh, int prime_len, int generator,
                            BN_GENCB *cb);
};

void fatal(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    printf(format, argptr);
    va_end(argptr);
    exit(1);
}

void report_progress(const char *what, bool okay)
{
    if (okay) {
        printf("%s: OK\n", what);
    } else {
        fatal("%s: FAILED\n", what);
    }
}

static bool i_am_server(void)
{
    /* TODO: Dynamically find out whether I am TLS server or TLS client. For now, assume server. */
    return true;
}

static int dh_generate_key(DH *dh)
{
    if (return_fixed_key_for_testing) {
        /* #### */
    }
    if (i_am_server()) {
        printf("dh_generate_key (server)\n");
        /* Set key_handle to "NULL" key handle, to request QKD_OPEN to allocate a key handle. */
        key_handle_t key_handle = {0};
        /* TODO: For now, the stub has a hard-coded assumption that the QKD server and client run
           on the same host, and the stub never even looks at the destination address. */
        ip_address_t destination = {
            .length = 0,
            .address = {0}
        };
        /* TODO: For now, set QoS to dummy values */
        qos_t qos = {
            .requested_length = 0,   /* TODO: This one we should probably set */
            .max_bps = 0,
            .priority = 0,
            .timeout = 0
        };
        enum RETURN_CODES result;
        result = QKD_OPEN(destination, qos, key_handle);
        report_progress("QKD_OPEN", QKD_RC_SUCCESS == result);

        /* TODO: Server side processing
           - The server calls QKD_OPEN() with a NULL key_handle, which generates a new
             key_handle, which is 64 octets.   **DONE***
           - For now, we only specify the requested_lengh as a QoS parameter. (How many bytes of
             shared secret does DH require?). For now, we don't specify max_bps or priority.
           - What timeout shall we use? Is OpenSSL tolerant to this function blocking for even a few
             ms? What happens is we specify a timeout of 0 (zero)?
           - The server uses the key_handle as the DH public key. This public key will be included
             in the Server-Hello message which plays the role of the SEND_KEY_HANDLE() operation in
             the ETSI sequence diagram. Note that this is why the ETSI document requires that "no
             key material can be derived from the handle" (top of page 9).
           - The server sets the DH private key to NULL. The QKD exchange does not use any DH
             private key on the server. */
    } else {
        printf("dh_generate_key (client)\n");
        /* TODO: Client side processing
           - The client uses the received DH public key as the ETSI API key_handle
           - The client calls QKD_OPEN() with the key_handle obtained it this way.
           - Note that the overloaded generate_key function does different things on the server
             side and the client side. Thus, there needs to be some way for the overloaded function
             to "know" whether it is being called in a server role or a client role. (How?)
           - The client uses the key_handle as it's own DH public key. In other words, the client's
             DH public key is the same as the server's DH public key.
           - The client sets its DH private key to NULL. Just as on the server, the QKD exchange
             does not use any DH private key on the client. */
    }
    /* I am not sure if we can synchronize the key_handle in this function already.
       We could calculate the 'lowest slot' on both sides of the link here
       and then agree on a key_handle in the compute function by taking the max. 
       That would mean no qkd api calls here.*/
    return 0;
}

static int dh_compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    /* See comment above. All qkd api calls would then happen here. */
    
    /* TODO: The overloaded compute_key function on the client does the following:
    - The client calls QKD_CONNECT_BLOCKING.
    - This call requires the IP address of the peer (the server in this case). 
      Does OpenSSL provide some API to get the IP address of the peer?
    - Once again, is OpenSSL tolerant to doing a blocking call here? I don't 
      think we can use the non-blocking variation (which is not well-defined in 
      the ETSI API document; there is no sequence diagram for it).
    - The client calls QKD_GET_KEY which returns a key_buffer. This is used as 
      the shared secret and returned. */

    /* TODO: The overloaded compute_key function on the server does the following:
    - The server verifies that the received client public key is equal to its 
      own server public key. If not, it fails. This is just a sanity check and 
      this step is not essential for guaranteeing the security of the procedure 
      (any attacker can easily spoof the public key).
    - The server calls QKD_CONNECT_BLOCKING. (See considerations on the client 
      side about needing the IP address of the peer, the client in this case, 
      and this call being blocking.)
    - The server calls QKD_GET_KEY which returns a key_buffer. This is used as 
      the shared secret and returned. Note that this returns the same key_buffer 
      as that which was return on the client side, so it is indeed a shared secret.s */
    return -1;  /* TODO */
}

static DH_METHOD example_dh_method = {
    .name = "Example DH Method",
    .generate_key = dh_generate_key,
    .compute_key = dh_compute_key,
    .bn_mod_exp = NULL,                     /* TODO */
    .init = NULL,                           /* TODO */
    .finish = NULL,                         /* TODO */
    .flags = 0,                             /* TODO */
    .app_data = NULL,                       /* TODO */
    .generate_params = NULL                 /* TODO */
};

int example_engine_init(ENGINE *engine)
{
    return 1;
}

int example_engine_bind(ENGINE *engine, const char *engine_id)
{
    int result = ENGINE_set_id(engine, example_engine_id);
    report_progress("ENGINE_set_id", result != 0);
    
    result = ENGINE_set_name(engine, example_engine_name);
    report_progress("ENGINE_set_name", result != 0);

    result = ENGINE_set_DH(engine, &example_dh_method);
    report_progress("ENGINE_set_DH", result != 0);

    result = ENGINE_set_init_function(engine, example_engine_init);
    report_progress("ENGINE_set_init_function", result != 0);

    return 1;
}

IMPLEMENT_DYNAMIC_CHECK_FN();
IMPLEMENT_DYNAMIC_BIND_FN(example_engine_bind);
