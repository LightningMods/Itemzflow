#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// We need to provide an export to force the expected stub library to be generated
int VerifyRSA(const char* file, const char* pubkkey)
{
	return Check_rsa(file, pubkkey);

}