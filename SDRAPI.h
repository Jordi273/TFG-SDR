/**
  *@file SDRAPI.h
  *@version 1.0
  *@date 13/04/2023
  *@author JordiCastilloValles
  */

/**
  *\mainpage Virtual Sdr API
  *
  *This API is designed to help developers create working code without having to worry about which physical platform will be using. For this reason some points have to be considered:
  *First, the signals transmitted and received will be in a scale from 0 to 1, the true output power will depend on the physichal system itself.
  *Second, some functions migth do nothing in your platform if your system doesn't need. For example, an SDR based on the Analog Devices AD9361 chip needs the ability to change the channel, but an SDR like the iotSDR doesn't have the. We even have the extreme case of an usb SDR, which only has one channel and one port, so there is no need to change channels or ports.
  *Third, some platforms migth not have it's code developed, so the use of this API is not adviced.
  *Fourth, the number of functions of the APi migth be increased in future versions, so being up-to-date could be important in order to get the maximum of the API.
  *Finally, any contribution to this project is xelcomed and could be extremely usefull.
  */

typedef enum
{
	RX = 0,
	TX = 1
} ChannelType;

/**
  *@brief In case the system works with multiplexers to choose from different hardware, this will represent the different options
  */


/**
  *@brief Error codes of this API 
  */
typedef enum{
	OK = 0,
	NOTIMPLEMENTED = 1,
	FILENOTOPEN = 2,
	NULLPOINTER = 3,
	NOCHANNEL = 4,
	NOPORT = 5,
	VALUEAPROXMIN = 6,
	VALUEAPROXMAX = 7,
	CHANNELNOTDEFINED = 8,
	REALSDRNOTFOUND,
	NEXTERROR 
} VirtualSdrError;


typedef enum
{
	A=65,
	B,
	C,
	D,
	E,
	F,
	G
} SdrChannel;

/**
  *@brief This parameter represents the physical port itself
  */
typedef enum
{
	first = 1,
	second,
	third,
	fourth,
	fifth,
	sixth
} SdrPort;
/**
  *@brief How the SDR is connected, through ip, usb... 
  */
typedef enum
{
	IP = 1,
	USB
} SdrConnectionType;

typedef enum
{
	OFF,
	ON
} SdrPortState;

typedef enum
{
	IIR,
	FIR
} FilterType;

typedef enum
{
	NOFUNCTION,
	TXONLYONCE,
	RXONLYONCE,
	TXCONTINUOUSLY,
	RXFILE,
	TXFILEONCE,
	TXFILECONTINUOUSLY
} SdrFunction;

/** 
  *@brief Handler of the API 
*/
struct VirtualSdr{
	char m_location[20];
	char m_RxChannel;
	char m_TxChannel;
	SdrConnectionType m_connectionType;
	long m_FS;
	struct PortList* m_ports;
	float** m_IList;
	float** m_QList;
	int* m_LengthBuffer;
	SdrFunction* m_function;
	char** m_fileName;
	
	void* m_RealSdr;
};

/**
  *@brief Handler of the configuration of the virtual sdr 
  */
struct SdrConfig{
	struct ChannelList* m_channels;
	struct PortList* m_ports;
	SdrConnectionType m_connectionType;
	char m_location[20];
	SdrChannel m_activeRxChannel;
	SdrChannel m_activeTxChannel;
	long m_minFS;
	long m_maxFS;
	long m_FS;
};

/**
  *@brief List for the Channel BSP information
  */
struct ChannelList{
	struct ChannelList* m_next;
	SdrChannel m_channel;
	ChannelType m_type;
	long m_minFS;
	long m_maxFS;
	long m_minFrec;
	long m_maxFrec;
	float m_minAmp;
	float m_maxAmp;
	int m_minBw;
	int m_maxBw;
};

struct PortList{
	struct PortList* m_next;
	SdrChannel m_channel;
	ChannelType m_type;
	SdrPort m_port;
	long m_Frec;
	float m_Amp;
	int m_Bw;
	SdrPortState m_state;
};

/**
  *@brief StartSdr Function that connects and configures the real Sdr as the virtual Sdr
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@return Error code with 0 as succes
  */
VirtualSdrError StartSdr(struct VirtualSdr*);

/**
  *@brief StartSdr Function that disconnects the real Sdr and the virtual Sdr
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@return Error code with 0 as succes
  */
VirtualSdrError StopSdr(struct VirtualSdr*);

/**
  *@brief ChargeConfig Charges the configuration the user edited in the SdrConfig handler, it can be used even if connected to the real Sdr
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrConfig* Pointer to the handler which contains the configuration desired
  *@return Error code with 0 as succes
*/
VirtualSdrError ChargeConfig(struct VirtualSdr*, struct SdrConfig*);

/**
  *@brief SetConnection Specifyes the connection between the computer and the real SDR
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrConnectionType How is the SDR connected to the PC
  *@param[in] char* String which has the direction of the SDR
  *@return Error code with 0 as succes
*/
VirtualSdrError SetConnection(struct SdrConfig*, SdrConnectionType, char*);

/**
  *@brief GetConnection Gets the actual connection between the computer and the real SDR described in the configuration
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[out] SdrConnectionType Buffer to store how is the SDR connected to the PC
  *@param[out] char* Buffer to store as a string the direction of the SDR
  *@return Error code with 0 as succes
*/
VirtualSdrError GetConnection(struct SdrConfig*, SdrConnectionType*, char*);

/**
  *@brief ChangeRxChannel Function to change the channel that the Sdr is using in the receiving chain
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrChannel Desired channel to set up the receiving chain
  *@return Error code with 0 as succes
  */
VirtualSdrError ChangeRxChannel(struct SdrConfig*, SdrChannel);

/**
  *@brief GetRxChannel Function to get the channel that the Sdr is using in the receiving chain
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[out] SdrChannel* Buffer to store which channel is being used by the receiving chain
  *@return Error code with 0 as succes
  */
VirtualSdrError GetRxChannel(struct SdrConfig*, SdrChannel*);

/**
  *@brief ChangeTxChannel Function to change the channel that the Sdr is using in the transmittion chain
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrChannel Desired channel to set up the transmittion chain
  *@return Error code with 0 as succes
  */
VirtualSdrError ChangeTxChannel(struct SdrConfig*, SdrChannel);

/**
  *@brief GetTxChannel Function to get the channel that the Sdr is using in the transmitting chain
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[out] SdrChannel* Buffer to store which channel is being used by the receiving chain
  *@return Error code with 0 as succes
  */
VirtualSdrError GetTxChannel(struct SdrConfig*, SdrChannel*);

/**
  *@brief SetRxBw Function to set the frecuency of the receiving chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Port desired to configure
  *@param[in] int Desired frecuency of the receiving chain
  *@return Error code with 0 as succes
  */
VirtualSdrError SetRxFrec(struct SdrConfig*, SdrPort, long);

/**
  *@brief GetRxBw Function to set the frecuency of the receiving chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Port desired to configure
  *@param[out] int* Buffer to store the frecuency of the receiving chain
  *@return Error code with 0 as succes
  */
VirtualSdrError GetRxFrec(struct SdrConfig*, SdrPort, long*);

/**
  *@brief SetTxBw Function to set the frecuency of the transmittion chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Port desired to configure
  *@param[in] int Desired frecuency of the transmittion chain
  *@return Error code with 0 as succes
  */
VirtualSdrError SetTxFrec(struct SdrConfig*, SdrPort, long);

/**
  *@brief SetTxBw Function to set the frecuency of the transmittion chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Port desired to configure
  *@param[out] int* Buffer to store the frecuency of the transmittion chain
  *@return Error code with 0 as succes
  */
VirtualSdrError GetTxFrec(struct SdrConfig*, SdrPort, long*);

/**
  *@brief SetRxBw Function to set the Bandwidth of the receiving chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Receiving port to configure the bandwidth
  *@param[in] int Desired bandwidth of the receiving chain
  *@return Error code with 0 as succes
  */
VirtualSdrError SetRxBw(struct SdrConfig*, SdrPort, int);

/**
  *@brief GetRxBw Function to get the Bandwidth of the receiving chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Receiving port to configure the bandwidth
  *@param[out] int* Buffer to store the bandwidth of the receiving chain
  *@return Error code with 0 as succes
  */
VirtualSdrError GetRxBw(struct SdrConfig*, SdrPort, int*);

/**
  *@brief SetTxBw Function to set the Bandwidth of the transmittion chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Transmittion port to configure the bandwidth
  *@param[in] int Desired bandwidth of the transmittion chain
  *@return Error code with 0 as succes
  */
VirtualSdrError SetTxBw(struct SdrConfig*, SdrPort, int);

/**
  *@brief GetTxBw Function to get the Bandwidth of the transmittion chain of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Transmittion port to configure the bandwidth
  *@param[out] int* Buffer to store the bandwidth of the transmittion chain
  *@return Error code with 0 as succes
  */
VirtualSdrError GetTxBw(struct SdrConfig*, SdrPort, int*);

/**
  *@brief SetGain Function to set the gain of the receiving port
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Receiving port to configure
  *@param[in] float* Desired gain of the port
  *@return Error code with 0 as succes
  */
VirtualSdrError SetGain(struct SdrConfig*, SdrPort, float*);
/**
  *@brief GetGain Function to get the gain of the receiving port
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Receiving port to configure
  *@param[out] float* Buffer to store the gain of the port
  *@return Error code with 0 as succes
  */
VirtualSdrError GetGain(struct SdrConfig*, SdrPort, float*);

/**
  *@brief SetAttenuation Function to set the attenuation of a transmittion port
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Transmition port to configure
  *@param[in] float* Attenuation desired of the port
  *@return Error code with 0 as succes
  */
VirtualSdrError SetAttenuation(struct SdrConfig*, SdrPort, float*);
/**
  *@brief GetAttenuation Function to get the attenuation of a transmittion port
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] SdrPort Transmition port to configure
  *@param[out] float* Buffer to store the attenuation of the port
  *@return Error code with 0 as succes
  */
VirtualSdrError GetAttenuation(struct SdrConfig*, SdrPort, float*);

/**
  *@brief Sets the sampling frecuency of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] int Sampling frecuency desired
  *@return Error code with 0 as succes
  */
VirtualSdrError SetFS(struct SdrConfig*, int);
/**
  *@brief Gets the sampling frecuency of the system
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[out] int* Buffer to store the sampling frecuency
  *@return Error code with 0 as succes
  */
VirtualSdrError GetFS(struct SdrConfig*, int*);

/**
  *@brief TransmitOnce Function to transmit only once the data from the buffers
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port to receive from
  *@param[in] int Length of the buffer
  *@param[in] float* Buffer of the I data
  *@param[in] float* Buffer of the Q data
  *@return Error code with 0 as succes
  */
  
VirtualSdrError TransmitOnce(struct VirtualSdr*, SdrPort, int, float*, float*);

/**
  *@brief TransmitAlways Function to transmit continuously the data from the buffers
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port to receive from
  *@param[in] int Length of the buffer
  *@param[in] float* Buffer of the I data
  *@param[in] float* Buffer of the Q data
  *@return Error code with 0 as succes
  */
VirtualSdrError TransmitAlways(struct VirtualSdr*, SdrPort, int, float*, float*);

/**
  *@brief Receive Function to receive data with the 0-1 format
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port which we want to use
  *@param[in] int Number of data wanted to receive
  *@param[out] float* Buffer of the I data
  *@param[out] float* Buffer of the Q data
  *@return Error code with 0 as succes
  */
VirtualSdrError Receive(struct VirtualSdr*, SdrPort, int, float*, float*);


/**
  *@brief SendSin Test function that sends a sinus from a port and checks if it's received correctly
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] float Amplitude of the sinus wanted to send, its range can be from 0 to 1
  *@param[in] SdrPort Port from where the test transmits
  *@return Error code with 0 as succes
  */
VirtualSdrError SendSin(struct VirtualSdr*, float, SdrPort);

/**
  *@brief FindCompressionPoint Finds the compression point of the receiver by using another port to transmit
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port from where the test transmits
  *@param[in] SdrPort Port from where the test receives
  *@param[out] float* Buffer for the attenuation where we find the 1dB compression point of the receiver
  *@return Error code with 0 as succes
  */
VirtualSdrError FindCompressionPoint(struct VirtualSdr*, SdrPort, SdrPort, float*);

/**
  *@brief FindIIP3 Finds the third order interception point of the receiver by using another port to transmit
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port from where the test transmits
  *@param[in] SdrPort Port from where the test receives
  *@param[out] float* Buffer for the attenuation where we find the IIP3 of the receiver
  *@return Error code with 0 as succes
  */
VirtualSdrError FindIIP3(struct VirtualSdr*, SdrPort, SdrPort, float*);

/**
  *@brief SaveConfiguration Saves the configuration created by the user to a file
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[out] char* String of the name of the file where the configuration will be stored
  *@return Error code with 0 as succes
  */
VirtualSdrError SaveConfiguration(struct SdrConfig*, char*);

/**
  *@brief LoadConfiguration Loads the configuration descripted in a file
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] char* String of the name of the file where the configuration is stored
  *@return Error code with 0 as succes
  */
VirtualSdrError LoadConfiguration(struct SdrConfig*, char*);

/**
  *@brief TransmitFromFileOnce Reads a file and transmits only one time its data
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port to transmit
  *@param[in] char* String of the name of the file to read the data from 
  *@return Error code with 0 as succes
  */
VirtualSdrError TransmitFromFileOnce(struct VirtualSdr*,SdrPort,char*);

/**
  *@brief TransmitFromFile Reads a file and transmits continuously its data
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port to transmit
  *@param[in] char* String of the name of the file to read the data from 
  *@return Error code with 0 as succes
  */
VirtualSdrError TransmitFromFile(struct VirtualSdr*,SdrPort, char*);

/**
  *@brief ReceiveToFile Reads a RX port and saves its data to a file
  *@param[in] VirtualSdr* Pointer to the Sdr from we want to read
  *@param[in] SdrPort Port to read from
  *@param[in] int Number of points to read
  *@param[out] char* String of the name of the file where we save the data
  *@return Error code with 0 as succes
  */
VirtualSdrError ReceiveToFile(struct VirtualSdr*, SdrPort, int, char*);


/**
  *@brief CheckPortState Function to know if a port is transmitting/receiving or not
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  *@param[in] SdrPort Port to heck the state
  *@param[in] ChannelType Check RX or TX ports
  *@param[out] SdrPortState* It can be ON if the port is trnasmitting/receiving or OFF if the port is inactive
  *@return Error code with 0 as succes
  */
VirtualSdrError CheckPortState(struct VirtualSdr*, SdrPort, ChannelType, SdrPortState*);

/**
  *@brief LoadBSP Loades a board support package which has the relevant information about your device
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] char* File that has the information about the Real Sdr data in csv format
  *@return Error code with 0 as succes
  */
VirtualSdrError LoadBSP(struct SdrConfig*, char*);

/**
  *@brief GetPLLParam Returns to the user the parameters of the pll
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use, it must be connected to the real SDR
  *@param[out] float* M parameter of the pll
  *@param[out] float* N parameter of the pll
  *@param[out] float* P parameter of the pll
  *@return Error code with 0 as succes
  */
VirtualSdrError GetPLLParam(struct VirtualSdr*, float*, float*, float*);

/**
  *@brief SetFilter 
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[in] FilterType* Which type of filter is going to be used
  *@return Error code with 0 as succes
  */
VirtualSdrError SetFilter (struct SdrConfig*, FilterType*);

/**
  *@brief GetFilter 
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  *@param[out] FilterType* Which type of filter is using
  *@return Error code with 0 as succes
  */
VirtualSdrError GetFilter (struct SdrConfig*, FilterType*);

/**
  *@brief PrintSdrConfig Function to print configuration
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  */
void PrintSdrConfig (struct SdrConfig*);

/**
  *@brief FreeSdrConfig Function to free all the memory allocated in the configuration handler
  *@param[in] SdrConfig* Pointer to the handler of the configuration
  */
void FreeSdrConfig (struct SdrConfig*);

/**
  *@brief PrintSdrConfig Function to print the SDR
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  */
void PrintVirtualSdr (struct VirtualSdr*);

/**
  *@brief FreeSdrConfig Function to free all the memory allocated in the SDR handler
  *@param[in] VirtualSdr* Pointer to the handler of the Virtual SDR to use
  */
void FreeVirtualSdr (struct VirtualSdr*);

/**
  *@brief PrintSdrErrorCode Function to print the error message in the terminal
  *@param[in] VirtualSdrError Error code returned from another function
  */
void PrintSdrErrorCode (VirtualSdrError);
