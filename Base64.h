#ifndef BASE64_H_
#define BASE64_H_

#include <string>

class Base64 {
public:
	static std::string Encode(const unsigned char* Data, int DataByte);
	static std::string Decode(const char* Data, int DataByte, int& OutByte);

private:
	static const char* EncodeTable; 
	static const char  DecodeTable[];
	
};

#endif // !BASE64_H_

