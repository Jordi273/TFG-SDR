#include "SDRAPI.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <iio.h>
#include <math.h>
 
 struct AD9361{
 	struct iio_context *m_ctx;
	struct iio_buffer  **m_rtxBuf;
 };
/* check return value of attr_write function */
static void errchk(int v, const char* what) {
	 if (v < 0) { fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what);}
}

/* write attribute: long long int */
static void wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
	errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}

/* write attribute: string */
static void wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
	errchk(iio_channel_attr_write(chn, what, str), what);
}

/*write attribute: double */
static void wr_ch_double(struct iio_channel *chn, const char* what, double val)
{
	errchk(iio_channel_attr_write_double(chn, what, val), what);
}

/* helper function generating channel names */
void get_ch_name(const char* type, int id, char* tmpstr)
{
	//printf("%d\n", id);
	//snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
	snprintf(tmpstr, sizeof(char)*64, "%s%d", type, id);
	//printf("%s,%d,%c\n",tmpstr,id,tmpstr[6]);
}

/* returns ad9361 phy device */
static struct iio_device* get_ad9361_phy(struct iio_context *ctx)
{
	struct iio_device *dev =  iio_context_find_device(ctx, "ad9361-phy");
	return dev;
}

/* finds AD9361 streaming IIO devices */
static bool get_ad9361_stream_dev(ChannelType d, struct iio_device **dev, struct iio_context *ctx)
{
	switch (d) {
	case TX: *dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc"); return *dev != NULL;
	case RX: *dev = iio_context_find_device(ctx, "cf-ad9361-lpc");  return *dev != NULL;
	default: return false;
	}
}

/* finds AD9361 streaming IIO channels */
static bool get_ad9361_stream_ch(ChannelType d, struct iio_device *dev, int chid, struct iio_channel **chn, char* tmpstr)
{
	get_ch_name("voltage", chid, tmpstr);
	*chn = iio_device_find_channel(dev, tmpstr, d == TX);
	if (!*chn)
		{
			get_ch_name("altvoltage", chid,tmpstr);
			*chn = iio_device_find_channel(dev, tmpstr, d == TX);
		}
	return *chn != NULL;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static bool get_phy_chan(ChannelType d, int chid, struct iio_channel **chn, char* tmpstr, struct iio_context *ctx)
{
	//printf("%d\n",chid);
	switch (d) {
	case RX: get_ch_name("voltage", chid ,tmpstr); *chn = iio_device_find_channel(get_ad9361_phy(ctx), tmpstr, false); return *chn != NULL;
	case TX: get_ch_name("voltage", chid, tmpstr); *chn = iio_device_find_channel(get_ad9361_phy(ctx), tmpstr, true);  return *chn != NULL;
	default: return false;
	}
}

/* finds AD9361 local oscillator IIO configuration channels */
static bool get_lo_chan(ChannelType d, struct iio_channel **chn, char* tmpstr, struct iio_context *ctx)
{
	switch (d) {
	 // LO chan is always output, i.e. true
	case RX: get_ch_name("altvoltage", 0, tmpstr); *chn = iio_device_find_channel(get_ad9361_phy(ctx), tmpstr, true); return *chn != NULL;
	case TX: get_ch_name("altvoltage", 1, tmpstr); *chn = iio_device_find_channel(get_ad9361_phy(ctx), tmpstr, true); return *chn != NULL;
	default: return false;
	}
}
 
VirtualSdrError StartSdr(struct VirtualSdr* virtual)
{

	/*
		Improvements:
			-Clean this mess
			-Solve the problem with files
			-Save the iio_context and iio_buffers in the null pointer to be cleaned in the stopSDR function
			-Add the part where we put the ports in ON --> Question, only put ON the nodes of always? if port is just only once how do you put later the OFF, is threading an option?
	*/	
	
	struct AD9361* auxVoidPointer;
	struct iio_context * auxContextAh;
	if(virtual == NULL)
	{
		return NULLPOINTER;
	}
	
	virtual->m_RealSdr = (struct AD9361 *) malloc(sizeof(struct AD9361));
	char auxContext [40];
	if(virtual->m_connectionType == USB)
	{
		strcpy(auxContext, "serial:");
		strcat(auxContext, virtual->m_location);
		auxContextAh = iio_create_context_from_uri(auxContext);
		
	}
	if(virtual->m_connectionType == IP)
	{
		strcpy(auxContext, "ip:");
		strcat(auxContext, virtual->m_location);
		//((struct AD9361*) virtual-->m_RealSdr).m_ctx = iio_create_context_from_uri(auxContext);
		auxVoidPointer = (struct AD9361*) virtual->m_RealSdr;
		auxContextAh = iio_create_context_from_uri(auxContext);	
	}
	if(auxContextAh == NULL)
	{
		return REALSDRNOTFOUND;
	}
	
	struct PortList* portIter = virtual->m_ports;
	char auxStr [64];
	char auxRxChannel [] = "X_BALANCED";
	char auxTxChannel [] = "X";
	int numberPorts = 0;
	
	while(portIter != NULL)
	{
		numberPorts++;
		struct iio_channel *chn = NULL;
		if (!get_phy_chan(portIter->m_type, portIter->m_port-1, &chn, auxStr, auxContextAh)) 
		{
			return REALSDRNOTFOUND; 
		}
		wr_ch_lli(chn, "rf_bandwidth", portIter->m_Bw);
		wr_ch_lli(chn, "sampling_frequency", virtual->m_FS);
		
		if(portIter->m_type == TX)
		{
			wr_ch_double(chn, "hardwaregain", portIter->m_Amp);
			auxTxChannel[0] = portIter->m_channel;
			wr_ch_str(chn, "rf_port_select", auxTxChannel);
		}
		else
		{
			if(portIter->m_Amp >= 0)
			{
				wr_ch_str(chn, "gain_control_mode", "manual");
				wr_ch_double(chn, "hardwaregain", portIter->m_Amp);
			}
			else
			{
				wr_ch_str(chn, "gain_control_mode", "fast_attack");
			}
			auxRxChannel[0] = portIter->m_channel;
			wr_ch_str(chn, "rf_port_select", auxRxChannel);
			
		}
		if (!get_lo_chan(portIter->m_type, &chn, auxStr,auxContextAh )) 
		{ 
			return REALSDRNOTFOUND; 
		}
		wr_ch_lli(chn, "frequency", portIter->m_Frec);
		
		portIter = portIter->m_next;
	}
	
	struct iio_device  *rtx;
	struct iio_channel *rtx_i;
	struct iio_channel *rtx_q;
	
	struct iio_buffer  **rtxbuf;
	rtxbuf = (struct iio_buffer**) malloc(numberPorts*sizeof(struct iio_buffer*));
	
	portIter = virtual->m_ports;
	
	char *p_dat, *p_end;
	ptrdiff_t p_inc;
	
	for(int i = 0; i < numberPorts; i++)
	{
		int t_iter = 0;
		switch(virtual->m_function[i])
		{
			case TXFILEONCE:
			case TXONLYONCE:
				if(!(get_ad9361_stream_dev(TX, &rtx, auxContextAh)))
				{
					return REALSDRNOTFOUND;
				}
				if(!(get_ad9361_stream_ch(TX, rtx, portIter->m_port*2, &rtx_i, auxStr)))
				{
					return REALSDRNOTFOUND;
				}
				if(!(get_ad9361_stream_ch(TX, rtx, portIter->m_port*2+1, &rtx_q, auxStr)))
				{
					return REALSDRNOTFOUND;
				}
				iio_channel_enable(rtx_i);
				iio_channel_enable(rtx_q);
				
				rtxbuf[i] = iio_device_create_buffer(rtx, virtual->m_LengthBuffer[i], false);
				if (!(rtxbuf[i])) {
					return REALSDRNOTFOUND;
				}
				printf("%p\n",rtxbuf[i]);			
				p_inc = iio_buffer_step(rtxbuf[i]);
				p_end = iio_buffer_end(rtxbuf[i]);
				for (p_dat = (char *)iio_buffer_first(rtxbuf[i], rtx_i); p_dat < p_end; p_dat += p_inc) {
					((int16_t*)p_dat)[0] = (int16_t) ((pow(2, 15)-1)*virtual->m_IList[i][t_iter]); // Real (I)
					((int16_t*)p_dat)[1] = (int16_t) ((pow(2, 15)-1)*virtual->m_QList[i][t_iter]); // Imag (Q)
					t_iter++;
				}
				break;
			case RXONLYONCE:
				if(!(get_ad9361_stream_dev(RX, &rtx, auxContextAh)))
				{
					return REALSDRNOTFOUND;
				}
				if(!(get_ad9361_stream_ch(RX, rtx, portIter->m_port*2, &rtx_i, auxStr)))
				{
					return REALSDRNOTFOUND;
				}
				if(!(get_ad9361_stream_ch(RX, rtx, portIter->m_port*2+1, &rtx_q, auxStr)))
				{
					return REALSDRNOTFOUND;
				}
				iio_channel_enable(rtx_i);
				iio_channel_enable(rtx_q);
				rtxbuf[i] = iio_device_create_buffer(rtx, virtual->m_LengthBuffer[i], false);
				break;
			case TXFILECONTINUOUSLY:
			case TXCONTINUOUSLY:
				if(!(get_ad9361_stream_dev(TX, &rtx, auxContextAh)))
				{
					return REALSDRNOTFOUND;
				}
				if(!(get_ad9361_stream_ch(TX, rtx, portIter->m_port*2, &rtx_i, auxStr)))
				{
					return REALSDRNOTFOUND;
				}
				if(!(get_ad9361_stream_ch(TX, rtx, portIter->m_port*2+1, &rtx_q, auxStr)))
				{
					return REALSDRNOTFOUND;
				}
				iio_channel_enable(rtx_i);
				iio_channel_enable(rtx_q);
				
				rtxbuf[i] = iio_device_create_buffer(rtx, virtual->m_LengthBuffer[i], true);
				if (!(rtxbuf[i])) {
					return REALSDRNOTFOUND;
				}
				
				p_inc = iio_buffer_step(rtxbuf[i]);
				p_end = iio_buffer_end(rtxbuf[i]);
				for (p_dat = (char *)iio_buffer_first(rtxbuf[i], rtx_i); p_dat < p_end; p_dat += p_inc) {
					((int16_t*)p_dat)[0] = (int16_t) ((pow(2, 15)-1)*virtual->m_IList[i][t_iter]); // Real (I)
					((int16_t*)p_dat)[1] = (int16_t) ((pow(2, 15)-1)*virtual->m_QList[i][t_iter]); // Imag (Q)
					t_iter++;
				}
				break;
		}
		portIter = portIter->m_next;
	}
	
	int bufferSent = 0;
	for(int i = 0; i < numberPorts; i++)
	{
		if(virtual->m_function[i] == TXCONTINUOUSLY || virtual->m_function[i] == TXONLYONCE)
		{
			bufferSent = iio_buffer_push(rtxbuf[i]);
		}
		if(virtual->m_function[i] == RXONLYONCE)
		{
			bufferSent = iio_buffer_refill(rtxbuf[i]);
		}
	}
	
	portIter = virtual->m_ports;
	for(int i = 0; i < numberPorts; i++)
	{
		if(virtual->m_function[i] == RXONLYONCE)
		{
			int t_iter = 0;
			p_inc = iio_buffer_step(rtxbuf[i]);
			p_end = iio_buffer_end(rtxbuf[i]);
			
			if(!(get_ad9361_stream_dev(RX, &rtx, auxContextAh)))
			{
				return REALSDRNOTFOUND;
			}
			if(!(get_ad9361_stream_ch(RX, rtx, portIter->m_port*2, &rtx_i, auxStr)))
			{
				return REALSDRNOTFOUND;
			}
			
			for (p_dat = (char *)iio_buffer_first(rtxbuf[i], rtx_i); p_dat < p_end; p_dat += p_inc) 
			{
				// Imag (Q) + Real (I)
				
				printf("%d --> %f, ",((int16_t*)p_dat)[0],virtual->m_IList[i][t_iter]);
				printf("%d --> %f\n",((int16_t*)p_dat)[1],virtual->m_QList[i][t_iter]);
				virtual->m_IList[i][t_iter] = (float)(((int16_t*)p_dat)[0])/(pow(2,11)-1);
				virtual->m_QList[i][t_iter] = (float)(((int16_t*)p_dat)[1])/(pow(2,11)-1);
				t_iter++;
			}
		}
		if(virtual->m_function[i] == RXFILE)
		{
			p_inc = iio_buffer_step(rtxbuf[i]);
			p_end = iio_buffer_end(rtxbuf[i]);
			
			if(!(get_ad9361_stream_dev(RX, &rtx, auxContextAh)))
			{
				return REALSDRNOTFOUND;
			}
			if(!(get_ad9361_stream_ch(RX, rtx, portIter->m_port*2, &rtx_i, auxStr)))
			{
				return REALSDRNOTFOUND;
			}
			
			FILE * stream = fopen(virtual->m_fileName[i], "r");
			if(stream == NULL)
			{
				return FILENOTOPEN;
			}
			
			for (p_dat = (char *)iio_buffer_first(rtxbuf[i], rtx_i); p_dat < p_end; p_dat += p_inc) 
			{
				// Imag (Q) + Real (I)
				fprintf(stream, "%f,%f\n",(float)((((int16_t*)p_dat)[0])>>4)/(pow(2,11)-1),(float)((((int16_t*)p_dat)[1])>>4)/(pow(2,11)-1));
			}
			fclose(stream);
		}
		portIter = portIter->m_next;
	}
	return OK;
}

VirtualSdrError StopSdr(struct VirtualSdr*)
{
	return 1;
}

VirtualSdrError ChargeConfig(struct VirtualSdr* virtual, struct SdrConfig* configuration)
{
	if(virtual == NULL || configuration == NULL)
	{
		return NULLPOINTER;
	}
	
	FreeVirtualSdr (virtual);
	
	virtual->m_connectionType = configuration->m_connectionType;
	strcpy(virtual->m_location, configuration->m_location);
	virtual->m_RxChannel = configuration->m_activeRxChannel;
	virtual->m_TxChannel = configuration->m_activeTxChannel;
	virtual->m_FS = configuration->m_FS;
	
	virtual->m_ports = NULL;
	struct PortList * portIter = configuration->m_ports;
	
	int bufferNeeded = 0;
	while(portIter != NULL && virtual->m_ports == NULL)
	{
		if((portIter->m_channel == virtual->m_RxChannel && portIter->m_type == RX)||(portIter->m_channel == virtual->m_TxChannel && portIter->m_type == TX))
		{
			virtual->m_ports = (struct PortList*) malloc(sizeof(struct PortList));
			memcpy(virtual->m_ports,portIter,sizeof(struct PortList));
			virtual->m_ports->m_state = OFF;
			virtual->m_ports->m_next = NULL;
			bufferNeeded++;
		}
		portIter = portIter->m_next;
	}
	
	struct PortList * virtualPortIter = virtual->m_ports;
	
	while(portIter != NULL)
	{
		if((portIter->m_channel == virtual->m_RxChannel && portIter->m_type == RX)||(portIter->m_channel == virtual->m_TxChannel && portIter->m_type == TX))
		{
			virtualPortIter->m_next = (struct PortList*) malloc(sizeof(struct PortList));
			memcpy(virtualPortIter->m_next ,portIter,sizeof(struct PortList));
			virtualPortIter = virtualPortIter->m_next;
			virtualPortIter->m_next = NULL;
			virtualPortIter->m_state = OFF;
			bufferNeeded++;
		}
		portIter = portIter->m_next;
	}
	
	virtual->m_IList = (float**)malloc(bufferNeeded*sizeof(float*));
	virtual->m_QList = (float**)malloc(bufferNeeded*sizeof(float*));
	virtual->m_LengthBuffer = (int*)malloc(bufferNeeded*sizeof(int));
	virtual->m_fileName = (char**)malloc(bufferNeeded*sizeof(char*));
	virtual->m_function = (SdrFunction*)malloc(bufferNeeded*sizeof(SdrFunction));
	
	for(int i = 0; i < bufferNeeded; i++)
	{
		virtual->m_IList[i] = NULL;
		virtual->m_QList[i] = NULL;
		virtual->m_LengthBuffer[i] = 0;
		virtual->m_function[i] = NOFUNCTION;
		virtual->m_fileName = NULL;
	}
	
	return 1;
}

VirtualSdrError SetConnection(struct SdrConfig* configuration, SdrConnectionType type, char* location)
{
	if(location == NULL || configuration == NULL)
	{
		return NULLPOINTER;
	}
	configuration->m_connectionType = type;
	strcpy(configuration->m_location,location);
	return OK;
}

VirtualSdrError GetConnection(struct SdrConfig* configuration, SdrConnectionType* type, char* location)
{
	if(location == NULL || type == NULL || configuration == NULL)
	{
		return NULLPOINTER;
	}
	*type = configuration->m_connectionType;
	strcpy(location,configuration->m_location);
	return OK;
}

VirtualSdrError ChangeRxChannel(struct SdrConfig* configuration, SdrChannel channel)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	bool found = false;
	struct ChannelList* iterator = configuration->m_channels;
	while(!found && iterator != NULL)
	{
		if(iterator->m_type == RX && iterator->m_channel == channel)
		{
			found = true;
		}
		else
		{
			iterator = iterator->m_next;
		}
	}
	if(!found)
	{
		return NOCHANNEL;
	}
	configuration->m_activeRxChannel = channel;
	return OK;
}

VirtualSdrError GetRxChannel(struct SdrConfig* configuration, SdrChannel* buffer)
{
	if(configuration == NULL || buffer == NULL)
	{
		return NULLPOINTER;
	}
	buffer = & configuration->m_activeRxChannel;
	return OK;
}

VirtualSdrError ChangeTxChannel(struct SdrConfig* configuration, SdrChannel channel)
{
		if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	bool found = false;
	struct ChannelList* iterator = configuration->m_channels;
	while(!found && iterator != NULL)
	{
		if(iterator->m_type == TX && iterator->m_channel == channel)
		{
			found = true;
		}
		else
		{
			iterator = iterator->m_next;
		}
	}
	if(!found)
	{
		return NOCHANNEL;
	}
	configuration->m_activeTxChannel = channel;
	return OK;
}

VirtualSdrError GetTxChannel(struct SdrConfig* configuration, SdrChannel* buffer)
{
	if(configuration == NULL || buffer == NULL)
	{
		return NULLPOINTER;
	}
	buffer = & configuration->m_activeTxChannel;
	return OK;
}

VirtualSdrError SetRxFrec(struct SdrConfig* configuration, SdrPort port, long f)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeRxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
		struct ChannelList* iterChannelList = configuration->m_channels;
	while (iterChannelList != NULL)
	{
		if(iterChannelList->m_channel==configuration->m_activeTxChannel)
		{
			struct PortList * aux = configuration->m_ports;
			while(aux != NULL)
			{
				if(aux->m_type == RX && aux->m_channel==configuration->m_activeRxChannel && aux->m_port == port)
				{
					if(f > iterChannelList->m_maxFrec)
					{
						aux->m_Frec = iterChannelList->m_maxFrec;
						return VALUEAPROXMAX;
					}
					if(f < iterChannelList->m_minFrec)
					{
						aux->m_Frec = iterChannelList->m_minFrec;
						return VALUEAPROXMIN;
					}
					aux->m_Frec = f;
					return OK;
				}
				aux = aux->m_next;
			}
			return NOPORT;
		}
		iterChannelList = iterChannelList->m_next;
	}
	return CHANNELNOTDEFINED;
}

VirtualSdrError GetRxFrec(struct SdrConfig* configuration, SdrPort port, long* f)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeRxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct PortList * aux = configuration->m_ports;
	while(aux != NULL)
	{
		if(aux->m_type == RX && aux->m_channel==configuration->m_activeRxChannel && aux->m_port == port)
		{
			f[0] = aux->m_Frec;
			return OK;
		}
		aux = aux->m_next;
	}
	f[0] = -1;
	return NOPORT;
}

VirtualSdrError SetTxFrec(struct SdrConfig* configuration, SdrPort port, long f)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeTxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct ChannelList* iterChannelList = configuration->m_channels;
	while (iterChannelList != NULL)
	{
		if(iterChannelList->m_channel==configuration->m_activeTxChannel)
		{
			struct PortList * aux = configuration->m_ports;
			while(aux != NULL)
			{
				if(aux->m_type == TX && aux->m_channel==configuration->m_activeTxChannel && aux->m_port == port)
				{
					if(f > iterChannelList->m_maxFrec)
					{
						aux->m_Frec = iterChannelList->m_maxFrec;
						return VALUEAPROXMAX;
					}
					if(f < iterChannelList->m_minFrec)
					{
						aux->m_Frec = iterChannelList->m_minFrec;
						return VALUEAPROXMIN;
					}
					aux->m_Frec = f;
					return OK;
				}
				aux = aux->m_next;
			}
			return NOPORT;
		}
		iterChannelList = iterChannelList->m_next;
	}
	return CHANNELNOTDEFINED;
}

VirtualSdrError GetTxFrec(struct SdrConfig* configuration, SdrPort port, long* f)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeTxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct PortList * aux = configuration->m_ports;
	while(aux != NULL)
	{
		if(aux->m_type == TX && aux->m_channel==configuration->m_activeTxChannel && aux->m_port == port)
		{
			f[0] = aux->m_Frec;
			return OK;
		}
		aux = aux->m_next;
	}
	f[0] = -1;
	return NOPORT;
}

VirtualSdrError SetRxBw(struct SdrConfig* configuration, SdrPort port, int bw) 
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeRxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
		struct ChannelList* iterChannelList = configuration->m_channels;
	while (iterChannelList != NULL)
	{
		if(iterChannelList->m_channel==configuration->m_activeTxChannel)
		{
			struct PortList * aux = configuration->m_ports;
			while(aux != NULL)
			{
				if(aux->m_type == RX && aux->m_channel==configuration->m_activeRxChannel && aux->m_port == port)
				{
					if(bw > iterChannelList->m_maxBw)
					{
						aux->m_Bw = iterChannelList->m_maxBw;
						return VALUEAPROXMAX;
					}
					if(bw < iterChannelList->m_minBw)
					{
						aux->m_Bw = iterChannelList->m_minBw;
						return VALUEAPROXMIN;
					}
					aux->m_Bw = bw;
					return OK;
				}
				aux = aux->m_next;
			}
			return NOPORT;
		}
		iterChannelList = iterChannelList->m_next;
	}
	return CHANNELNOTDEFINED;
}

VirtualSdrError GetRxBw(struct SdrConfig* configuration, SdrPort port, int* bw)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeRxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct PortList * aux = configuration->m_ports;
	while(aux != NULL)
	{
		if(aux->m_type == RX && aux->m_channel==configuration->m_activeRxChannel && aux->m_port == port)
		{
			bw[0] = aux->m_Bw;
			return OK;
		}
		aux = aux->m_next;
	}
	bw[0] = -1;
	return NOPORT;
}

VirtualSdrError SetTxBw(struct SdrConfig* configuration, SdrPort port, int bw)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeTxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct ChannelList* iterChannelList = configuration->m_channels;
	while (iterChannelList != NULL)
	{
		if(iterChannelList->m_channel==configuration->m_activeTxChannel)
		{
			struct PortList * aux = configuration->m_ports;
			while(aux != NULL)
			{
				if(aux->m_type == TX && aux->m_channel==configuration->m_activeTxChannel && aux->m_port == port)
				{
					if(bw > iterChannelList->m_maxBw)
					{
						aux->m_Bw = iterChannelList->m_maxBw;
						return VALUEAPROXMAX;
					}
					if(bw < iterChannelList->m_minBw)
					{
						aux->m_Bw = iterChannelList->m_minBw;
						return VALUEAPROXMIN;
					}
					aux->m_Bw = bw;
					return OK;
				}
				aux = aux->m_next;
			}
			return NOPORT;
		}
		iterChannelList = iterChannelList->m_next;
	}
	return CHANNELNOTDEFINED;
}

VirtualSdrError GetTxBw(struct SdrConfig* configuration, SdrPort port, int* bw)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeTxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct PortList * aux = configuration->m_ports;
	while(aux != NULL)
	{
		if(aux->m_type == TX && aux->m_channel==configuration->m_activeTxChannel && aux->m_port == port)
		{
			bw[0] = aux->m_Bw;
			return OK;
		}
		aux = aux->m_next;
	}
	bw[0] = -1;
	return NOPORT;
}
VirtualSdrError SetGain(struct SdrConfig* configuration, SdrPort port, float* gain)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeRxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
		struct ChannelList* iterChannelList = configuration->m_channels;
	while (iterChannelList != NULL)
	{
		if(iterChannelList->m_channel==configuration->m_activeTxChannel)
		{
			struct PortList * aux = configuration->m_ports;
			while(aux != NULL)
			{
				if(aux->m_type == RX && aux->m_channel==configuration->m_activeRxChannel && aux->m_port == port)
				{
					if(gain[0] > iterChannelList->m_maxAmp)
					{
						aux->m_Amp = iterChannelList->m_maxAmp;
						return VALUEAPROXMAX;
					}
					if(gain[0] < iterChannelList->m_minAmp)
					{
						aux->m_Amp = iterChannelList->m_minAmp;
						return VALUEAPROXMIN;
					}
					aux->m_Amp = gain[0];
					return OK;
				}
				aux = aux->m_next;
			}
			return NOPORT;
		}
		iterChannelList = iterChannelList->m_next;
	}
	return CHANNELNOTDEFINED;
}

VirtualSdrError GetGain(struct SdrConfig* configuration, SdrPort port, float* gain)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeRxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct PortList * aux = configuration->m_ports;
	while(aux != NULL)
	{
		if(aux->m_type == RX && aux->m_channel==configuration->m_activeRxChannel && aux->m_port == port)
		{
			gain[0] = aux->m_Amp;
			return OK;
		}
		aux = aux->m_next;
	}
	gain[0] = -1;
	return NOPORT;
}

VirtualSdrError SetAttenuation(struct SdrConfig* configuration, SdrPort port, float* Att)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeTxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
		struct ChannelList* iterChannelList = configuration->m_channels;
	while (iterChannelList != NULL)
	{
		if(iterChannelList->m_channel==configuration->m_activeTxChannel)
		{
			struct PortList * aux = configuration->m_ports;
			while(aux != NULL)
			{
				if(aux->m_type == TX && aux->m_channel==configuration->m_activeTxChannel && aux->m_port == port)
				{
					if(Att[0] > iterChannelList->m_maxAmp)
					{
						aux->m_Amp = iterChannelList->m_maxAmp;
						return VALUEAPROXMAX;
					}
					if(Att[0] < iterChannelList->m_minAmp)
					{
						aux->m_Amp = iterChannelList->m_minAmp;
						return VALUEAPROXMIN;
					}
					aux->m_Amp = Att[0];
					return OK;
				}
				aux = aux->m_next;
			}
			return NOPORT;
		}
		iterChannelList = iterChannelList->m_next;
	}
	return CHANNELNOTDEFINED;
}
 
VirtualSdrError GetAttenuation(struct SdrConfig* configuration, SdrPort port, float* Att)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_activeTxChannel == 0)
	{
		return CHANNELNOTDEFINED;
	}
	struct PortList * aux = configuration->m_ports;
	while(aux != NULL)
	{
		if(aux->m_type == TX && aux->m_channel==configuration->m_activeTxChannel && aux->m_port == port)
		{
			Att[0] = aux->m_Amp;
			return OK;
		}
		aux = aux->m_next;
	}
	Att[0] = -1;
	return NOPORT;
}

VirtualSdrError SetFS(struct SdrConfig* configuration, int desiredValue)
{
	if(configuration == NULL)
	{
		return NULLPOINTER;
	}
	if(configuration->m_minFS > desiredValue)
	{
		configuration->m_FS = configuration->m_minFS;
		return VALUEAPROXMIN;
	}
	else
	{
		if(configuration->m_maxFS < desiredValue)
		{
			configuration->m_FS = configuration->m_maxFS;
			return VALUEAPROXMAX;
		}
		else
		{
			configuration->m_FS = desiredValue;
		}
	}
	return OK;
}

VirtualSdrError GetFS(struct SdrConfig* configuration, int* buffer)
{
	if(configuration == NULL || buffer == NULL)
	{
		return NULLPOINTER;
	}
	buffer[0] = configuration->m_FS;
	return OK;
}

VirtualSdrError TransmitOnce(struct VirtualSdr* virtual, SdrPort port, int len, float* I, float*Q)
{
	if(virtual == NULL || I == NULL || Q == NULL)
	{
		return NULLPOINTER;
	}
	struct PortList* portIter = virtual->m_ports;
	int iter = 0;
	while(portIter != NULL){
		if(portIter->m_type == TX && portIter->m_port == port)
		{
			virtual->m_IList[iter] = I;
			virtual->m_QList[iter] = Q;
			virtual->m_LengthBuffer[iter] = len;
			virtual->m_function[iter] = TXONLYONCE;
			return OK;
		}
		iter++;
		portIter = portIter->m_next;
	}
	return NOPORT;
}

VirtualSdrError TransmitAlways(struct VirtualSdr* virtual, SdrPort port, int len, float* I, float* Q)
{
	if(virtual == NULL || I == NULL || Q == NULL)
	{
		return NULLPOINTER;
	}
	struct PortList* portIter = virtual->m_ports;
	int iter = 0;
	while(portIter != NULL){
		if(portIter->m_type == TX && portIter->m_port == port)
		{
			virtual->m_IList[iter] = I;
			virtual->m_QList[iter] = Q;
			virtual->m_LengthBuffer[iter] = len;
			virtual->m_function[iter] = TXCONTINUOUSLY;
			return OK;
		}
		iter++;
		portIter = portIter->m_next;
	}
	return NOPORT;
}

VirtualSdrError Receive(struct VirtualSdr* virtual, SdrPort port, int len, float* I, float* Q)
{
	if(virtual == NULL || I == NULL || Q == NULL)
	{
		return NULLPOINTER;
	}
	struct PortList* portIter = virtual->m_ports;
	int iter = 0;
	while(portIter != NULL){
		if(portIter->m_type == RX && portIter->m_port == port)
		{
			virtual->m_IList[iter] = I;
			virtual->m_QList[iter] = Q;
			virtual->m_LengthBuffer[iter] = len;
			virtual->m_function[iter] = RXONLYONCE;
			return OK;
		}
		iter++;
		portIter = portIter->m_next;
	}
	return NOPORT;
}

VirtualSdrError SendSin(struct VirtualSdr*, float, SdrPort)
{
	return 1;
}

VirtualSdrError FindCompressionPoint(struct VirtualSdr*, SdrPort, SdrPort, float*)
{
	return 1;
}

VirtualSdrError FindIIP3(struct VirtualSdr*, SdrPort, SdrPort, float*)
{
	return 1;
}

VirtualSdrError SaveConfiguration(struct SdrConfig*, char*)
{
	return 1;
}

VirtualSdrError LoadConfiguration(struct SdrConfig*, char*)
{
	return 1;
}

VirtualSdrError TransmitFromFileOnce(struct VirtualSdr* virtual ,SdrPort port ,char* dataFile)
{
	if(virtual == NULL)
	{
		return NULLPOINTER;
	}
	
	FILE* stream = fopen(dataFile, "r");
	if(stream == NULL)
	{
		return FILENOTOPEN;
	}
	int counterLines = 0;
	char line [64];
	while(NULL != fgets(line, 64, stream))
	{
		counterLines++;
	}
	fclose(stream);
	
	struct PortList* portIter = virtual->m_ports;
	int iter = 0;
	while(portIter != NULL){
		if(portIter->m_type == TX && portIter->m_port == port)
		{
			virtual->m_function[iter] = TXFILEONCE;
			virtual->m_fileName[iter] = (char*) malloc(sizeof(char)*strlen(dataFile));
			strcpy(virtual->m_fileName[iter], dataFile);
			virtual->m_IList[iter] = (float*) malloc(counterLines*sizeof(float));
			virtual->m_QList[iter] = (float*) malloc(counterLines*sizeof(float));
			
			stream = fopen(dataFile, "r");
			if(stream == NULL)
			{
				return FILENOTOPEN;
			}
			
			for(int i = 0; i < counterLines; i++)
			{
				char* tok;
				char* param;

				param = strdup(line);
		    		tok = strtok(line, ",");				
				virtual->m_IList[iter][i] = atof(tok);
				tok = strtok(NULL, ",\n");
				virtual->m_QList[iter][i] = atof(tok);
			}
			fclose(stream);
			return OK;
		}
		iter++;
		portIter = portIter->m_next;
	}
	
	return NOPORT;
}

VirtualSdrError TransmitFromFile(struct VirtualSdr* virtual,SdrPort port, char* dataFile)
{
	if(virtual == NULL)
	{
		return NULLPOINTER;
	}
	
	FILE* stream = fopen(dataFile, "r");
	if(stream == NULL)
	{
		return FILENOTOPEN;
	}
	int counterLines = 0;
	char line [64];
	while(NULL != fgets(line, 64, stream))
	{
		counterLines++;
	}
	fclose(stream);
	
	struct PortList* portIter = virtual->m_ports;
	int iter = 0;
	while(portIter != NULL){
		if(portIter->m_type == TX && portIter->m_port == port)
		{
			virtual->m_function[iter] = TXFILECONTINUOUSLY;
			virtual->m_fileName[iter] = (char*) malloc(sizeof(char)*strlen(dataFile));
			strcpy(virtual->m_fileName[iter], dataFile);
			virtual->m_IList[iter] = (float*) malloc(counterLines*sizeof(float));
			virtual->m_QList[iter] = (float*) malloc(counterLines*sizeof(float));
			
			stream = fopen(dataFile, "r");
			if(stream == NULL)
			{
				return FILENOTOPEN;
			}
			
			for(int i = 0; i < counterLines; i++)
			{
				char* tok;
				char* param;

				param = strdup(line);
		    		tok = strtok(line, ",");				
				virtual->m_IList[iter][i] = atof(tok);
				tok = strtok(NULL, ",\n");
				virtual->m_QList[iter][i] = atof(tok);
			}
			fclose(stream);
			return OK;
		}
		iter++;
		portIter = portIter->m_next;
	}
	
	return NOPORT;
}

VirtualSdrError ReceiveToFile(struct VirtualSdr* virtual, SdrPort port, int dataLen, char* dataFile)
{
	if(virtual == NULL)
	{
		return NULLPOINTER;
	}
	
	struct PortList* portIter = virtual->m_ports;
	int iter = 0;
	while(portIter != NULL){
		if(portIter->m_type == RX && portIter->m_port == port)
		{
			virtual->m_function[iter] = RXFILE;
			virtual->m_fileName[iter] = (char*) malloc(sizeof(char)*strlen(dataFile));
			strcpy(virtual->m_fileName[iter], dataFile);
			virtual->m_LengthBuffer[iter] = dataLen;
			virtual->m_IList[iter] = (float*)malloc(dataLen*sizeof(float));
			virtual->m_QList[iter] = (float*)malloc(dataLen*sizeof(float));
			return OK;
		}
		iter++;
		portIter = portIter->m_next;
	}
	
	return NOPORT;
}

VirtualSdrError CheckPortState(struct VirtualSdr* virtual, SdrPort port, ChannelType type, SdrPortState* buffer)
{
	if(virtual == NULL || buffer == NULL)
	{
		return NULLPOINTER;
	}
	
	struct PortList* iterPort = virtual->m_ports;
	while(iterPort != NULL)
	{
		if(iterPort->m_type == type && iterPort->m_port == port)
		{
			buffer[0] = iterPort->m_state;
			return OK;
		}
		iterPort = iterPort->m_next;
	}
	return NOPORT;
}

VirtualSdrError LoadBSP(struct SdrConfig* configuration, char* csvChannelFile)
{
	if (csvChannelFile == NULL || configuration == NULL)
	{
		return NULLPOINTER;
	}
	
	FreeSdrConfig (configuration);
	
	
	struct ChannelList* ListBuffer, * ListIndex;
	char* csvParam;
	char line[1024];
	long minFS = 10000000000;
	long maxFS = 0;
	
	FILE* stream = fopen(csvChannelFile, "r");
	if(stream == NULL)
	{
		return FILENOTOPEN;
	}
	//This needs to be changed, find new way of doing it cleaner, if not just put this garbage in a function
	if(NULL != fgets(line, 1024, stream))
	{
		
		csvParam = strdup(line);
		
		ListBuffer = (struct ChannelList*) malloc(sizeof(struct ChannelList));
		char* tok;

    		ListBuffer->m_next = NULL;
    		tok = strtok(line, ",");
    		
    		ListBuffer->m_channel = (int) *tok;
    		
    		tok = strtok(NULL, ",\n");
    		ListBuffer->m_type = atoi(tok);
		tok = strtok(NULL, ",\n");
		if(atol(tok) < minFS)
		{
			minFS = atol(tok);
		}
    		
    		tok = strtok(NULL, ",\n");		
    		if(atol(tok) > maxFS)
		{
			maxFS = atol(tok);
		}	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_minFrec = atol(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_maxFrec = atol(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_minAmp = atof(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_maxAmp = atof(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_minBw = atoi(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_maxBw = atoi(tok);
    		
    		tok = strtok(NULL, ",\n");
    		for(int i = 1; i < atoi(tok)+1; i++)
    		{	
    			struct PortList* portIterator = configuration->m_ports;
    			if(configuration->m_ports == NULL)
    			{	
    				configuration->m_ports = (struct PortList*) malloc(sizeof(struct PortList));
    				portIterator = configuration->m_ports;
    				portIterator->m_next = NULL;
    			}
    			else
    			{
    				while (portIterator->m_next != NULL)
	    			{
	    				portIterator = portIterator->m_next;
	    			}
	    			portIterator->m_next = (struct PortList*) malloc(sizeof(struct PortList));
    				portIterator = portIterator->m_next;
    				portIterator->m_next = NULL;
    			}
    			portIterator->m_channel = ListBuffer->m_channel;
    			portIterator->m_type = ListBuffer->m_type;
    			portIterator->m_port = i;
    			
    		}
    		
    		configuration->m_channels = ListBuffer;
    		ListIndex = ListBuffer;
	}
	while(NULL != fgets(line, 1024, stream))
	{
		csvParam = strdup(line);
		
		ListBuffer = (struct ChannelList*) malloc(sizeof(struct ChannelList));
		
		char* tok;

    		ListBuffer->m_next = NULL;
    		tok = strtok(line, ",");
    		
    		ListBuffer->m_channel = (int) *tok;
    		
    		tok = strtok(NULL, ",\n");
    		ListBuffer->m_type = atoi(tok);
    		
		tok = strtok(NULL, ",\n");
		if(atol(tok) < minFS)
		{
			minFS = atol(tok);
		}
    		
    		tok = strtok(NULL, ",\n");		
    		if(atol(tok) > maxFS)
		{
			maxFS = atol(tok);
		}	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_minFrec = atol(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_maxFrec = atol(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_minAmp = atof(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_maxAmp = atof(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_minBw = atoi(tok);	
    		
    		tok = strtok(NULL, ",\n");		
    		ListBuffer->m_maxBw = atoi(tok);
    		
    		tok = strtok(NULL, ",\n");
    		for(int i = 1; i < atoi(tok)+1; i++)
    		{
    			struct PortList* portIterator = configuration->m_ports;
    			if(configuration->m_ports == NULL)
    			{
    				configuration->m_ports = (struct PortList*) malloc(sizeof(struct PortList));
    				portIterator = configuration->m_ports;
    				portIterator->m_next = NULL;
    			}
    			else
    			{
    				while (portIterator->m_next != NULL)
	    			{
	    				portIterator = portIterator->m_next;
	    			}
	    			
	    			portIterator->m_next = (struct PortList*) malloc(sizeof(struct PortList));
    				portIterator = portIterator->m_next;
    				portIterator->m_next = NULL;
    			}
    			portIterator->m_channel = ListBuffer->m_channel;
    			portIterator->m_type = ListBuffer->m_type;
    			portIterator->m_port = i;
    		}
    		
    		ListIndex->m_next = ListBuffer;
    		ListIndex = ListBuffer;

	}
	
	fclose(stream);
	
	configuration->m_minFS = minFS;
	configuration->m_maxFS = maxFS;
	
	return OK;
}

VirtualSdrError GetPLLParam(struct VirtualSdr*, float*, float*, float*)
{
	return 1;
}
VirtualSdrError SetFilter (struct SdrConfig*, FilterType*)
{
	return 1;
}
VirtualSdrError GetFilter (struct SdrConfig*, FilterType*)
{
	return 1;
}
void PrintSdrConfig (struct SdrConfig* configuration)
{
	struct ChannelList* auxChannel = configuration->m_channels;
	while(auxChannel != NULL)
	{
		if(auxChannel->m_type == RX)
		{
			printf("Receiving channel %c has a minimum Flo of %ld and a maximum Flo of %ld, has a minimum Bw of %i and a maximum Bw of %i, has a minimum gain of %f and a maximum gain of %f\n", auxChannel->m_channel, auxChannel->m_minFrec, auxChannel->m_maxFrec, auxChannel->m_minBw, auxChannel->m_maxBw, auxChannel->m_minAmp, auxChannel->m_maxAmp);
		}
		if(auxChannel->m_type == TX)
		{
			printf("Transmitting channel %c has a minimum Flo of %ld and a maximum Flo of %ld, has a minimum Bw of %i and a maximum Bw of %i, has a minimum gain of %f and a maximum gain of %f\n", auxChannel->m_channel, auxChannel->m_minFrec, auxChannel->m_maxFrec, auxChannel->m_minBw, auxChannel->m_maxBw, auxChannel->m_minAmp, auxChannel->m_maxAmp);
		}
		auxChannel = auxChannel->m_next;
	}

	struct PortList* auxPort = configuration->m_ports;
	while(auxPort != NULL)
	{
		if(auxPort->m_type == RX)
		{
			printf("Receiving port %i from channel %c has a frecuency of %ld, a gain of %f and a bandwidth of %i\n", auxPort->m_port, auxPort->m_channel, auxPort->m_Frec, auxPort->m_Amp, auxPort->m_Bw);
		}
		if(auxPort->m_type == TX)
		{
			printf("Transmitting port %i from channel %c has a frecuency of %ld, a gain of %f and a bandwidth of %i\n", auxPort->m_port, auxPort->m_channel, auxPort->m_Frec, auxPort->m_Amp, auxPort->m_Bw);
		}
		auxPort = auxPort->m_next;
	}

	if(configuration->m_connectionType == IP)
	{
		printf("Connected to the IP %s\n", configuration->m_location);
	}
	else
	{
		if(configuration->m_connectionType == USB)
		{
			printf("Connected to the usb %s\n", configuration->m_location);
		}
		else
		{
			printf("Connection not defined yet\n");
		}
	}
	printf("The minimum sampling frecuency is %ld and the maximum is %ld, the actual value is %ld\n", configuration->m_minFS, configuration->m_maxFS, configuration->m_FS);
	printf("Active RX channel: %c\nActive TX channel: %c\n", configuration->m_activeRxChannel, configuration->m_activeTxChannel);
}

void FreeSdrConfig (struct SdrConfig* configuration)
{
	if(configuration != NULL)
	{
		struct ChannelList* temp;
		while (configuration->m_channels != NULL)
		{
			temp = configuration->m_channels;
			configuration->m_channels = configuration->m_channels->m_next;
			free(temp);
		}
		struct PortList* aux;
		while (configuration->m_ports != NULL)
		{
			aux = configuration->m_ports;
			configuration->m_ports = configuration->m_ports->m_next;
			free(aux);
		}
	}
}

void PrintVirtualSdr (struct VirtualSdr* virtual)
{
	if(virtual != NULL)
	{
		printf("Virtual SDR:\n");
		if(virtual->m_connectionType == IP)
		{
			printf("Real SDR connected through IP at %s\n", virtual->m_location);
		}
		if(virtual->m_connectionType == USB)
		{
			printf("Real SDR connected through USB at %s\n", virtual->m_location);
		}
		printf("Receiving channel %c and transmitting channel %c working with a sampling frecuency of %ld\n",virtual->m_RxChannel, virtual->m_TxChannel, virtual->m_FS);
		struct PortList* portIter = virtual->m_ports;
		while(portIter != NULL)
		{
			if(portIter->m_type == RX)
			{
				printf("Receiving channel %i working with a Flo of %ld, a bandwidth of %i and a gain of %f\n", portIter->m_port, portIter->m_Frec, portIter->m_Bw, portIter->m_Amp);
			}
			if(portIter->m_type == TX)
			{
				printf("Transmitting channel %i working with a Flo of %ld, a bandwidth of %i and a gain of %f\n", portIter->m_port, portIter->m_Frec, portIter->m_Bw, portIter->m_Amp);
			}
			portIter = portIter->m_next;
		}
	}
}

void FreeVirtualSdr (struct VirtualSdr* virtual)
{
	if(virtual != NULL)
	{
		struct PortList * auxPortList;
		int i = 0;
		while (virtual->m_ports != NULL)
		{
			auxPortList = virtual->m_ports;
			virtual->m_ports = virtual->m_ports->m_next;
			free(auxPortList);
			
			if(virtual->m_function[i] == TXFILEONCE || virtual->m_function[i] == TXFILECONTINUOUSLY)
			{
				free(virtual->m_IList[i]);
				free(virtual->m_QList[i]);
				free(virtual->m_fileName[i]);
			}
			i++;
		}
		free(virtual->m_IList);
		free(virtual->m_QList);
		free(virtual->m_function);
		free(virtual->m_RealSdr);
		free(virtual->m_LengthBuffer);
	}
}

void PrintSdrErrorCode (VirtualSdrError error)
{
	switch(error)
	{
		case OK: 
			printf("Everything went OK\n");
			break;
		case NOTIMPLEMENTED: 
			printf("Function not implemented\n");
			break;  
		case FILENOTOPEN: 
			printf("File can't be opened\n");
			break; 
		case NULLPOINTER: 
			printf("Pointer with NULL value given\n");
			break;
		case NOCHANNEL: 
			printf("Channel doesn't exist\n");
			break; 
		case NOPORT: 
			printf("Port doesn't exist\n");
			break; 
		case VALUEAPROXMIN: 
			printf("Desired value not in the interval, approximating to the minimum set in the BSP file\n");
			break;  
		case VALUEAPROXMAX: 
			printf("Desired value not in the interval, approximating to the maximum set in the BSP file\n");
			break; 
		case CHANNELNOTDEFINED: 
			printf("There is no active channel defined\n");
			break; 
		case REALSDRNOTFOUND: 
			printf("Real Sdr cannot be found\n");
			break; 
		default:
			printf("Error code doesn't exist, check if everything is OK with your program\n");
			break; 
	}
}
