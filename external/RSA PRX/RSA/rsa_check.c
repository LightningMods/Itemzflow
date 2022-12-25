#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mbedtls/config.h"
#include "mbedtls/rsa.h"
#include "mbedtls/md.h"
#include "rsa_check.h"
#include <orbis/libkernel.h>



void logshit(char* format, ...)
{

    char buff[1024];

    memset(buff, 0, 1024);

    va_list args;
    va_start(args, format);
    vsprintf(buff, format, args);
    va_end(args);

    printf(buff);

    int fd = sceKernelOpen("/data/Loader_Logs.txt", O_WRONLY | O_CREAT | O_APPEND, 0777);
    if (fd >= 0)
    {

        sceKernelWrite(fd, buff, strlen(buff));
        sceKernelClose(fd);
    }
}


int Check_rsa(const char * path, const char * pubkey)
{
   FILE *f;
    int ret = 1;
    unsigned c;
    int exit_code = -1;
    size_t i;
    mbedtls_rsa_context rsa;
    unsigned char hash[32];
    unsigned char buf[MBEDTLS_MPI_MAX_SIZE];
    char filename[512];

    logshit("[%s:%i] ----- INIT ARMmbed with RSA_PKCS_V15  ---\n", __FUNCTION__, __LINE__);


    mbedtls_rsa_init( &rsa, MBEDTLS_RSA_PKCS_V15, 0 );

 
    logshit("[%s:%i] ----- Reading public key from %s (rsa.pub) ---\n", __FUNCTION__, __LINE__, pubkey);

    if( ( f = fopen( pubkey, "rb" ) ) == NULL )
    {
        logshit("[%s:%i] ----- failed to read rsa.pub ---\n", __FUNCTION__, __LINE__);
        goto exit;
    }



    if( ( ret = mbedtls_mpi_read_file( &rsa.N, 16, f ) ) != 0 ||
        ( ret = mbedtls_mpi_read_file( &rsa.E, 16, f ) ) != 0 )
    {
        
        logshit("[%s:%i] ----- mbedtls_mpi_read_file returned %d ---\n", __FUNCTION__, __LINE__, ret);
        fclose( f );
        exit_code = -ret;
        goto exit;
    }


    rsa.len = ( mbedtls_mpi_bitlen( &rsa.N ) + 7 ) >> 3;

    fclose( f );

    /*
     * Extract the RSA signature from the text file
     */
 
    snprintf(filename, sizeof(filename), "%s.sig", path);
    if( ( f = fopen( filename, "rb" ) ) == NULL )
    {
          logshit("[%s:%i] ----- Could NOT open sigfile ---\n", __FUNCTION__, __LINE__);
        goto exit;
    }

    i = 0;
    while( fscanf( f, "%02X", (unsigned int*) &c ) > 0 &&
           i < (int) sizeof( buf ) )
        buf[i++] = (unsigned char) c;

    fclose( f );

    if( i != rsa.len )
    {
        logshit("[%s:%i] ----- Invaild RSA Sign format ---\n", __FUNCTION__, __LINE__);
        exit_code = 0x999;
        goto exit;
    }

    /*
     * Compute the SHA-256 hash of the input file and
     * verify the signature
     */
 
    logshit("[%s:%i] ----- Verifying the RSA/SHA-256 signature ---\n", __FUNCTION__, __LINE__);
 

    if( ( ret = mbedtls_md_file(
                    mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ),
                    path, hash ) ) != 0 )
    {
         logshit("[%s:%i] ----- could not read file ---\n", __FUNCTION__, __LINE__);
        // fflush( stdout );
        goto exit;
    }

    if( ( ret = mbedtls_rsa_pkcs1_verify( &rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC,
                                  MBEDTLS_MD_SHA256, 20, hash, buf ) ) != 0 )
    {
        

        logshit("[%s:%i] -----  mbedtls_rsa_pkcs1_verify returned -0x%0x ---\n", __FUNCTION__, __LINE__, -ret);
        exit_code = -ret;
        goto exit;
    }

 
    logshit("[%s:%i] ----- Success the signature is valid  ---\n", __FUNCTION__, __LINE__);



    exit_code = 0;

exit:

    mbedtls_rsa_free( &rsa );

    return exit_code;

}