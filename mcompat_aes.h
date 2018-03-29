#ifndef __INAES_H__
#define __INAES_H__

// AES only supports Nb=4
#define Nb 4			// number of columns in the State & expanded key
#define Nk 4			// number of columns in a key
#define Nr 10			// number of rounds in encryption


void Encrypt(unsigned char *InText, unsigned char *ExpKey, unsigned char *OutText);
void Decrypt(unsigned char *InText, unsigned char *ExpKey, unsigned char *OutText);


bool AES_Encrypt(unsigned char *Key, unsigned char *InText, int len, unsigned char *OutText);

bool AES_Decrypt(unsigned char *Key, unsigned char *InText, int len, unsigned char *OutText);


#endif /* __GETWORK_H__ */
