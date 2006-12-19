static int EmulateScanFull16_176Interpolate(unsigned int scan,unsigned short *data)
{
	unsigned short *ps=NULL,*end=NULL;
	unsigned char *pd=NULL;
	int xpitch=0;
	TInt retValue = 0;
	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	scan = gLineTable[scan];
	
	if ((int)scan< 0) return 0; // Out of range
	if ((int)scan>=176) return 0; // Out of range
	
	pd=Targ.screen+gLineOffsets[scan];//Targ.screen+scan*2+Targ.screen_offset+8;
	
	xpitch=-Targ.scanline_length;
	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;
		// Reduce 14 pixels into 9
		do
		{
			*(unsigned short *)pd=gColorMapTab[ps[0]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[1]]+gColorMapTab[ps[2]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[3]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[4]]+gColorMapTab[ps[5]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[6]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[7]]+gColorMapTab[ps[8]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[9]]+gColorMapTab[ps[10]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[11]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[12]]+gColorMapTab[ps[13]])>>1);pd+=xpitch;
			ps+=14;
		}
		while (ps<end);
	}
	else
	{
		ps=data+32; end=ps+256;
		
		// Reduce 5 pixels into 4
		do
		{
			*(unsigned short *)pd=gColorMapTab[ps[0]];pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[1]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[2]]+gColorMapTab[ps[3]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[4]];pd+=xpitch;
			ps+=5;
		}
		while (ps<end);
	}
	return retValue;
}

static int EmulateScanFullRight16_176Interpolate(unsigned int scan,unsigned short *data)
{
	unsigned short *ps=NULL,*end=NULL;
	unsigned char *pd=NULL;
	int xpitch=0;
	int retValue = 0;
	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	scan = gLineTable[scan];

	if ((int)scan< 0) return 0; // Out of range
	if ((int)scan>=176) return 0; // Out of range
	
	pd=Targ.screen+gLineOffsets[scan];//Targ.screen+Targ.scanline_length-scan*2-8;
	
	xpitch=+Targ.scanline_length;
	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;
		// Reduce 14 pixels into 9
		do
		{
			*(unsigned short *)pd=gColorMapTab[ps[0]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[1]]+gColorMapTab[ps[2]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[3]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[4]]+gColorMapTab[ps[5]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[6]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[7]]+gColorMapTab[ps[8]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[9]]+gColorMapTab[ps[10]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[11]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[12]]+gColorMapTab[ps[13]])>>1);pd+=xpitch;
			ps+=14;
		}
		while (ps<end);
	}
	else
	{
		ps=data+32; end=ps+256;
		// Reduce 5 pixels into 4
		do
		{
			*(unsigned short *)pd=gColorMapTab[ps[0]];pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[1]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[2]]+gColorMapTab[ps[3]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[4]];pd+=xpitch;
			ps+=5;
		}
		while (ps<end);
	}
	
	return retValue;
}



static int EmulateScan16_176Interpolate(unsigned int scan,unsigned short *data)
{
	unsigned short *ps=NULL,*end=NULL;
	unsigned char *pd=NULL;
	int xpitch=0;
	int retValue = 0;
	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	scan = gLineTable[scan];
	
	if ((int)scan< 0) return 0; // Out of range
	if ((int)scan>=176) return 0; // Out of range
	
	pd=Targ.screen+gLineOffsets[scan];//Targ.screen+scan*Targ.scanline_length;
	
	xpitch=2;
	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;
		// Reduce 9 pixels into 5
		do
		{
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[0]]+gColorMapTab[ps[1]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[2]]+gColorMapTab[ps[3]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[4]]+gColorMapTab[ps[5]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[6]]+gColorMapTab[ps[7]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[8]];pd+=xpitch;
			ps+=9;
		}
		while (ps<end);
	} 
	else
	{
		ps=data+32; end=ps+256;
		// Reduce 10 pixels into 7
		do
		{
			*(unsigned short *)pd=gColorMapTab[ps[0]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[1]]+gColorMapTab[ps[2]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[3]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[4]]+gColorMapTab[ps[5]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[6]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[7]]+gColorMapTab[ps[8]])>>1);pd+=xpitch;	
			*(unsigned short *)pd=gColorMapTab[ps[9]];pd+=xpitch;
			ps+=10;
		}
		while (ps<end);  
	}
	
	return retValue;
}

static int EmulateStretchScan16_176Interpolate(unsigned int scan,unsigned short *data)
{
	unsigned short *ps=NULL,*end=NULL;
	unsigned char *pd=NULL;
	int xpitch=0;
	int retValue = 0;
	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	scan = gLineTable[scan];

	if ((int)scan<0) 
		return 0; // Out of range
	if ((int)scan>=208) 
		return 0; // Out of range
	
	pd=Targ.screen+gLineOffsets[scan];//Targ.screen+scan*Targ.scanline_length;
	
	xpitch=2;
	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;
		// Reduce 9 pixels into 5
		do
		{
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[0]]+gColorMapTab[ps[1]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[2]]+gColorMapTab[ps[3]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[4]]+gColorMapTab[ps[5]])>>1);pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[6]]+gColorMapTab[ps[7]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[8]];pd+=xpitch;
			ps+=9;
		}
		while (ps<end);
	}
	else
	{
		ps=data+32; end=ps+256;
		// Reduce 10 pixels into 7
		do
		{
			*(unsigned short *)pd=gColorMapTab[ps[0]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[1]]+gColorMapTab[ps[2]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[3]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[4]]+gColorMapTab[ps[5]])>>1);pd+=xpitch;
			*(unsigned short *)pd=gColorMapTab[ps[6]];pd+=xpitch;
			*(unsigned short *)pd=(unsigned short)((gColorMapTab[ps[7]]+gColorMapTab[ps[8]])>>1);pd+=xpitch;	
			*(unsigned short *)pd=gColorMapTab[ps[9]];pd+=xpitch;
			ps+=10;
		}
		while (ps<end);  
	}
	
	return retValue;
}
