#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>


//compile with the -llsl flag
//gcc venti.c -lssl -o venti
// gcc venti.c -lcrypt -o venti

int main(int argn, char *argv[]) {

  int i;
	unsigned char temp[SHA_DIGEST_LENGTH];
	char buf[SHA_DIGEST_LENGTH*2];

	memset(buf, 0x0, SHA_DIGEST_LENGTH*2);
	memset(temp, 0x0, SHA_DIGEST_LENGTH);

  unsigned char data[] = "/butts/crumbs";
	SHA1(data, strlen(data), temp);

	for (i=0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf((char*)&(buf[i*2]), "%02x", temp[i]);
	}

	printf("SHA1 of %s is %s\n", data, buf);



//   char data[] = "/butts/crumbs";
//   size_t length = sizeof(data);
//
//   unsigned char* hash[SHA_DIGEST_LENGTH];
//   SHA1(data, length, hash);
// // hash now contains the 20-byte SHA-1 hash
//   printf("fingerprint: %s\n", hash);
//   printf("Size of digest: %d\n", SHA_DIGEST_LENGTH);
	return 0;

}
