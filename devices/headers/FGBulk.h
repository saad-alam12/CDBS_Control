/*
 * USBBulkBridge.h
 *
 *  Created on: Feb 16, 2016
 *      Author: Francesco Guatieri
 */

#ifndef FGBULK_H_
#define FGBULK_H_

typedef bool (* BulkBridgeCallback)(void *, unsigned char, unsigned char *, unsigned int);

class FGBulkBridge
{
private:
	void *            Parameters;
	BulkBridgeCallback WriteCallback;
	BulkBridgeCallback ReadCallback;

public:
	FGBulkBridge() :
		Parameters(nullptr), WriteCallback(nullptr), ReadCallback(nullptr) {};

	FGBulkBridge(void *P, BulkBridgeCallback WC, BulkBridgeCallback RC) :
		Parameters(P), WriteCallback(WC), ReadCallback(RC) {};

	bool Write(unsigned char Endpoint, unsigned char *Buffer, unsigned int Length)
	{
		//cout << "Requented write to endpoint #" << (int)Endpoint << "\n";
		if(WriteCallback == nullptr) {
			if(Verbosity > 2) Shout("Write callback not set for bulk bridge to device.");
			return false;
		};

		return WriteCallback(Parameters, Endpoint, Buffer, Length);
	};

	bool Read(unsigned char Endpoint, unsigned char *Buffer, unsigned int Length)
	{
		if(ReadCallback == nullptr) {
			if(Verbosity > 2) Shout("Read callback not set for bulk bridge to device.");
			return false;
		};

		return ReadCallback(Parameters, Endpoint, Buffer, Length);
	};

	bool Query(unsigned char Endpoint, unsigned char *Buffer, unsigned int wLength, unsigned int rLength = ~0)
	{
		//cout << "Requested query to endpoint #" << (int)Endpoint << "\n";
		if(rLength == ~0) rLength = wLength;
		if(!Write(Endpoint, Buffer, wLength)) return false;
		return Read(Endpoint, Buffer, rLength);
	};
};


#endif /* FGBULK_H_ */
