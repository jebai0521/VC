// Find alive hosts
// Create by Dennis 
// 2013/04/09

#include "stdafx.h"
#include "ARPThread.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <process.h>
#include <string>
#include <algorithm>
#include <vector>
using namespace std;

#include "logMsg.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

CRITICAL_SECTION  cs;

unsigned long netIp = 0;

vector<st_ip_mac> g_vtIpMac;

// Get host IP
// Reference: http://blog.csdn.net/happycock/article/details/491424
// If you machine have more than one network adapter, you should
// fix this function.
void GetHostIP(char* ip, int size1, char* mask, int size2)
{
	ULONG len = 0; 
	GetAdaptersInfo(NULL, &len);
	PIP_ADAPTER_INFO p = static_cast<PIP_ADAPTER_INFO>(malloc(len));
	GetAdaptersInfo(p, &len);
	for (PIP_ADAPTER_INFO q = p; q != NULL; q = q->Next)
	{
		if (ip) strncpy(ip, q->IpAddressList.IpAddress.String, size1);
		if (mask) strncpy(mask, q->IpAddressList.IpMask.String, size2);
	}

	free(p);
}

unsigned int __stdcall ArpThread(LPVOID lParam)
{
	ULONG MacAddr[2];       /* for 6-byte hardware addresses */
	ULONG PhysAddrLen = 6;  /* default to length of six bytes */

	EnterCriticalSection(&cs);
	//converts to TCP/IP network byte order (which is big-endian) 
	unsigned long dstIP=htonl(++netIp);
	LeaveCriticalSection(&cs);

	// Reference: http://msdn.microsoft.com/en-us/library/aa366358(VS.85).aspx
	if (NO_ERROR == SendARP(dstIP, 0,&(MacAddr[0]),&PhysAddrLen)) {
		EnterCriticalSection(&cs);
		BYTE* bPhysAddr = (BYTE *) & MacAddr;
		if (PhysAddrLen) {
			struct in_addr addr1;
			memcpy(&addr1, &dstIP, 4);
#if 0
			printf("%16s : ", inet_ntoa(addr1));
			printf"%.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n"
				, bPhysAddr[0], bPhysAddr[1], bPhysAddr[2], bPhysAddr[3], bPhysAddr[4], bPhysAddr[5]);
#else
			char arrTmp[18] = {0};
			sprintf(arrTmp, "%.2X-%.2X-%.2X-%.2X-%.2X-%.2X"
				, bPhysAddr[0], bPhysAddr[1], bPhysAddr[2], bPhysAddr[3], bPhysAddr[4], bPhysAddr[5]);

			st_ip_mac ip_mac;
			ip_mac.ip = inet_ntoa(addr1);
			ip_mac.mac = arrTmp;

			g_vtIpMac.push_back(ip_mac);
#endif
		}
		LeaveCriticalSection(&cs);
	}

	return 0;
}

int IPFind(string strIP, string strMask, vector<st_ip_mac> &vtIpMac)
{  
	// Clear
	g_vtIpMac.swap(vector<st_ip_mac>());

	char strSrcIP[16] = {0};
	char strSrcMask[16] = {0};
	// Get Host IP and Mask
#if 0
	//GetHostIP(strSrcIP, sizeof(strSrcIP), strSrcMask, sizeof(strSrcMask));
#else
	memset(strSrcIP, 0, sizeof(strSrcIP));
	memset(strSrcMask, 0, sizeof(strSrcMask));
	strcpy(strSrcIP, strIP.c_str());
	strcpy(strSrcMask, strMask.c_str());
#endif
	InitializeCriticalSection(&cs);

	IPAddr SrcIp = inet_addr(strSrcIP);
	unsigned long findMask=inet_addr(strSrcMask);
	int netsize = ~ntohl(findMask);
	netIp = ntohl(SrcIp & findMask); 

	HANDLE *phThread = (HANDLE*)malloc(netsize*sizeof(HANDLE));

	for (int i=1; i<netsize; i++)
	{
		// _beginthreadex if more effect than CreateThread
		// you can google it for more information
		phThread[i-1] = (HANDLE)_beginthreadex(NULL, 0, ArpThread, 0, 0, NULL);
	}

	// There is a limit count for function WaitForMultipleObjects
	int netcount = netsize-1;
	int waitcount = 0;
	for (int i=0; i<netcount;)
	{
		waitcount = MAXIMUM_WAIT_OBJECTS;
		if (netcount-i<MAXIMUM_WAIT_OBJECTS)
			waitcount = netcount-i;
		WaitForMultipleObjects(waitcount,&(phThread[i]),TRUE,INFINITE);
		i += waitcount;
	}

	free(phThread);

	DeleteCriticalSection(&cs);

	// Copy result
	st_ip_mac ipmac;
	vtIpMac.resize(g_vtIpMac.size(),ipmac);
	copy(g_vtIpMac.begin(), g_vtIpMac.end(), vtIpMac.begin());

	return 0;
}