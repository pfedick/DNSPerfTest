/*
 * dstress.h
 *
 *  Created on: 27.09.2018
 *      Author: patrickf
 */

#ifndef DSTRESS_INCLUDE_DSTRESS_H_
#define DSTRESS_INCLUDE_DSTRESS_H_

#include <ppl7.h>
#include <ppl7-inet.h>
#include "dnsperftest_sensor.h"
//#include "udpecho.h"

PPL7EXCEPTION(MissingCommandlineParameter, Exception);
PPL7EXCEPTION(InvalidCommandlineParameter, Exception);
PPL7EXCEPTION(InvalidDNSQuery, Exception);
PPL7EXCEPTION(UnknownRRType, Exception);
PPL7EXCEPTION(BufferOverflow, Exception);
PPL7EXCEPTION(UnknownDestination, Exception);
PPL7EXCEPTION(InvalidQueryFile, Exception);
PPL7EXCEPTION(UnsupportedIPFamily, Exception);
PPL7EXCEPTION(FailedToInitializePacketfilter, Exception);

struct DNS_HEADER
{
    unsigned short id; // identification number

    unsigned char rd :1; // recursion desired
    unsigned char tc :1; // truncated message
    unsigned char aa :1; // authoritive answer
    unsigned char opcode :4; // purpose of message
    unsigned char qr :1; // query/response flag

    unsigned char rcode :4; // response code
    unsigned char cd :1; // checking disabled
    unsigned char ad :1; // authenticated data
    unsigned char z :1; // its z! reserved
    unsigned char ra :1; // recursion available

    unsigned short q_count; // number of question entries
    unsigned short ans_count; // number of answer entries
    unsigned short auth_count; // number of authority entries
    unsigned short add_count; // number of resource entries
};


int MakeQuery(const ppl7::String &query, unsigned char *buffer, size_t buffersize, bool dnssec=false, int udp_payload_size=4096);
unsigned short getQueryTimestamp();
double getQueryRTT(unsigned short start);

class Packet
{
private:
	unsigned char *buffer;
	int buffersize;
	int payload_size;
	bool chksum_valid;

	void updateChecksums();
public:
	Packet();
	~Packet();
	void setSource(const ppl7::IPAddress &ip_addr, int port);
	void setDestination(const ppl7::IPAddress &ip_addr, int port);
	void setPayload(const void *payload, size_t size);
	void setPayloadDNSQuery(const ppl7::String &query, bool dnssec=false);
	void setDnsId(unsigned short id);
	void setIpId(unsigned short id);

	void randomSourceIP(const ppl7::IPNetwork &net);
	void randomSourceIP(unsigned int start, unsigned int size);
	void randomSourcePort();

	size_t size() const;
	unsigned char* ptr();

};

class RawSocketSender
{
private:
	void *buffer;
	int sd;
public:
	RawSocketSender();
	~RawSocketSender();
	void setDestination(const ppl7::IPAddress &ip_addr, int port);
	ssize_t send(Packet &pkt);
	ppl7::SockAddr getSockAddr() const;
	bool socketReady();
};

class RawSocketReceiver
{
private:
	ppl7::IPAddress SourceIP;
	unsigned char *buffer;
	int buflen;
	int sd;
	unsigned short SourcePort;
#ifdef __FreeBSD__
	bool useZeroCopyBuffer;
#endif

public:
	RawSocketReceiver();
	~RawSocketReceiver();
	void initInterface(const ppl7::String &Device);
	bool socketReady();
	void setSource(const ppl7::IPAddress &ip_addr, int port);
#ifdef __FreeBSD__
	void receive(ppluint64 &num_pkgs, ppluint64 &bytes_rcv, double &rtt, double &min, double &max);
#else
	bool receive(size_t &size, double &rtt);
#endif
};


class PayloadFile
{
private:
	ppl7::File QueryFile;
	ppl7::Mutex QueryMutex;
	ppluint64 validLinesInQueryFile;
public:
	PayloadFile();
	void openQueryFile(const ppl7::String &Filename);
	void getQuery(ppl7::String &buffer);
};

class DNSReceiverThread : public ppl7::Thread
{
	private:
		RawSocketReceiver Socket;
		ppluint64 counter_packets_received;
		ppluint64 counter_bytes_received;
		double total_rtt;
		double min_rtt, max_rtt;



	public:
		DNSReceiverThread();
		~DNSReceiverThread();
		void setInterface(const ppl7::String &Device);
		void setSource(const ppl7::IPAddress &ip, int port);
		void run();

		ppluint64 getPacketsReceived() const;
		ppluint64 getBytesReceived() const;

		double getDuration() const;
		double getRoundTripTimeAverage() const;
		double getRoundTripTimeMin() const;
		double getRoundTripTimeMax() const;

};

class DNSSender
{
	public:
		class Results
		{
			public:
				int			queryrate;
				ppluint64	counter_send;
				ppluint64	counter_received;
				ppluint64	bytes_send;
				ppluint64	bytes_received;
				ppluint64	counter_errors;
				ppluint64	packages_lost;
				ppluint64   counter_0bytes;
				ppluint64   counter_errorcodes[255];
				double		duration;
				double		rtt_total;
				double		rtt_min;
				double		rtt_max;
				Results();
				void clear();
		};


	private:
		ppl7::ThreadPool threadpool;
		ppl7::IPAddress TargetIP;
		ppl7::IPAddress SourceIP;
		ppl7::IPNetwork SourceNet;
		ppl7::String CSVFileName;
		ppl7::String QueryFilename;
		ppl7::File CSVFile;
		ppl7::Array rates;
		ppl7::String InterfaceName;
		PayloadFile payload;
		DNSReceiverThread *Receiver;
		DNSSender::Results vis_prev_results;
		SystemStat sys1,sys2;

		int TargetPort;
		int Laufzeit;
		int Timeout;
		int ThreadCount;
		int DnssecRate;
		float Zeitscheibe;
		bool ignoreResponses;
		bool spoofingEnabled;

		void openCSVFile(const ppl7::String &Filename);
		void run(int queryrate);
		void presentResults(const DNSSender::Results &result);
		void saveResultsToCsv(const DNSSender::Results &result);
		void prepareThreads();
		void getResults(DNSSender::Results &result);
		ppl7::Array getQueryRates(const ppl7::String &QueryRates);
		void readSourceIPList(const ppl7::String &filename);

		void getTarget(int argc, char**argv);
		void getSource(int argc, char**argv);
		int getParameter(int argc, char**argv);
		int openFiles();

		void showCurrentStats(ppl7::ppl_time_t start_time);

	public:
		DNSSender();
		~DNSSender();
		void help();
		int main(int argc, char**argv);
};

DNSSender::Results operator-(const DNSSender::Results &first, const DNSSender::Results &second);

class DNSSenderThread : public ppl7::Thread
{
	private:
		ppl7::ByteArray buffer;
		RawSocketSender Socket;
		Packet pkt;

		ppl7::IPAddress destination;
		ppl7::IPAddress sourceip;
		ppl7::IPNetwork sourcenet;

		PayloadFile *payload;
		ppluint64 queryrate;
		ppluint64 counter_packets_send, errors, counter_0bytes;
		ppluint64 counter_bytes_send;
		ppluint64 counter_errorcodes[255];

		unsigned int spoofing_net_start;
		unsigned int spoofing_net_size;

		int runtime;
		int timeout;
		int DnssecRate;
		int dnsseccounter;
		double Zeitscheibe;

		double duration;
		bool spoofingEnabled;
		bool verbose;

		void sendPacket();
		void waitForTimeout();
		bool socketReady();

		void runWithoutRateLimit();
		void runWithRateLimit();

	public:
		DNSSenderThread();
		~DNSSenderThread();
		void setDestination(const ppl7::IPAddress &ip, int port);
		void setSourceIP(const ppl7::IPAddress &ip);
		void setSourceNet(const ppl7::IPNetwork &net);
		void setRandomSource(const ppl7::IPNetwork &net);
		void setRuntime(int seconds);
		void setTimeout(int seconds);
		void setDNSSECRate(int rate);
		void setQueryRate(ppluint64 qps);
		void setZeitscheibe(float ms);
		void setVerbose(bool verbose);
		void setPayload(PayloadFile &payload);
		void run();
		ppluint64 getPacketsSend() const;
		ppluint64 getBytesSend() const;
		ppluint64 getErrors() const;
		ppluint64 getCounter0Bytes() const;
		ppluint64 getCounterErrorCode(int err) const;

};



#endif /* DSTRESS_INCLUDE_DSTRESS_H_ */
