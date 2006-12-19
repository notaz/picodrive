static int EmulateScanFull16(unsigned int scan,unsigned short *data)
{
	unsigned short *ps=NULL,*end=NULL;
	unsigned short *pd=NULL;
	unsigned short *pdSrc1 = NULL;
	unsigned short *pdSrc2 = NULL;
	int screenScan;

	int index = 0;
	int xpitch=0;
	TInt retValue = 0;
	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	screenScan = gLineTable[scan];
	
	if ((int)screenScan< 0) return 0; // Out of range
	if ((int)screenScan>=Targ.view.iBr.iY) return 0; // Out of range
	
	pd=(unsigned short*)(Targ.screen+screenScan*2+Targ.screen_offset);
	pdSrc1 = pd;
	
	xpitch=-(Targ.scanline_length>>1);
	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;
		do
		{
			if(gColumnStepTable[index]>1)
			{
				*pd = gColorMapTab[*ps];
				pd+=xpitch;
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);	
				index++;
				ps++;
			}
			else if(gColumnStepTable[index]>0)
			{
				*pd = gColorMapTab[*ps];
				index++;
				ps++;				
			}
			else
			{
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
				ps+=2;
				index+=2;
			}
			pd+=xpitch;
		}
		while (ps<end);
	}
	else
	{
		ps=data+32; end=ps+256;
		
		// Reduce 10 pixels into 7
		do
		{
			if(gNarrowColumnStepTable[index]>1)
			{
				*pd = gColorMapTab[*ps];
				pd+=xpitch;
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);		
				index++;
			}
			else if(gNarrowColumnStepTable[index]>0)
			{
				*pd = gColorMapTab[*ps];
				ps++;
				index++;
			}
			else
			{
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
				ps+=2;
				index+=2;
			}
			pd+=xpitch;		
		}
		while (ps<end);  	
	}

	if(scan>0 && screenScan != gLineTable[scan-1]+1)
		{		
			pdSrc2 = pdSrc1-2;
			pd = pdSrc1-1;

			for(TInt loop=0;loop<Targ.view.iBr.iY;loop++)
			{
				*pd=((*pdSrc1+*pdSrc2)>>1);
				pd+=xpitch;
				pdSrc1+=xpitch;
				pdSrc2+=xpitch;
			}
			
		}

	return retValue;
}

static int EmulateScanFullRight16(unsigned int scan,unsigned short *data)
{
	unsigned short *ps=NULL,*end=NULL;
	unsigned short *pd=NULL;
	unsigned short *pdSrc1 = NULL;
	unsigned short *pdSrc2 = NULL;
	int screenScan;
	int xpitch=0;
	int retValue = 0;
	int index = 0;
	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	screenScan = gLineTable[scan];

	if ((int)screenScan< 0) return 0; // Out of range
	if ((int)screenScan>=Targ.view.iBr.iY) return 0; // Out of range
	
	pd=(unsigned short*)(Targ.screen+Targ.scanline_length-screenScan*2);
	pdSrc1 = pd;
	
	xpitch=+(Targ.scanline_length>>1);
	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;
		do
		{
			if(gColumnStepTable[index]>1)
			{
				*pd = gColorMapTab[*ps];
				pd+=xpitch;
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);	
				index++;
				ps++;
			}
			else if(gColumnStepTable[index]>0)
			{
				*pd = gColorMapTab[*ps];
				index++;
				ps++;				
			}
			else
			{
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
				ps+=2;
				index+=2;
			}
			pd+=xpitch;
		}
		while (ps<end);
	}
	else
	{
		ps=data+32; end=ps+256;
		
		// Reduce 10 pixels into 7
		do
		{
			if(gNarrowColumnStepTable[index]>1)
			{
				*pd = gColorMapTab[*ps];
				pd+=xpitch;
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);		
				index++;
			}
			else if(gNarrowColumnStepTable[index]>0)
			{
				*pd = gColorMapTab[*ps];
				ps++;
				index++;
			}
			else
			{
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
				ps+=2;
				index+=2;
			}
			pd+=xpitch;		
		}
		while (ps<end);  	
	}

	if(scan>0 && screenScan != gLineTable[scan-1]+1)
		{		
			pdSrc2 = pdSrc1+2;
			pd = pdSrc1+1;

			for(TInt loop=0;loop<Targ.view.iBr.iY;loop++)
			{
				*pd=((*pdSrc1+*pdSrc2)>>1);
				pd+=xpitch;
				pdSrc1+=xpitch;
				pdSrc2+=xpitch;
			}
			
		}
	
	return retValue;
}



static int EmulateScan16(unsigned int scan,unsigned short *data)
{
	//  int len=0;
	unsigned short *ps=NULL,*end=NULL;
	unsigned short *pd=NULL;
	int xpitch=0;
	int retValue = 0;
	int index = 0;

	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	scan = gLineTable[scan];
	
	if ((int)scan< 0) return 0; // Out of range
	if ((int)scan>=Targ.view.iBr.iY) return 0; // Out of range
	
	pd=(unsigned short*)(Targ.screen+scan*Targ.scanline_length);
	
	xpitch=2;
	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;
		do
		{
			if(gColumnStepTable[index]>0)
			{
			*pd = gColorMapTab[*ps];
			ps++;
		
			index++;
			}
			else
			{
			*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
			ps+=2;
			index+=2;
			}
			pd++;
		
		}
		while (ps<end);
	} 
	else
	{
		ps=data+32; end=ps+256;
		// Reduce 10 pixels into 7
		do
		{
			if(gNarrowColumnStepTable[index]>0)
			{
				*pd = gColorMapTab[*ps];
				ps++;
				index++;
			}
			else
			{
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
				ps+=2;
				index+=2;
			}
			pd++;		
		}
		while (ps<end);
	
	}
	
	return retValue;
}

static int EmulateStretchScan16(unsigned int scan,unsigned short *data)
{
	unsigned short *ps=NULL,*end=NULL;
	unsigned short *pd=NULL;
	unsigned short *pdSrc1 = NULL;
	unsigned short *pdSrc2 = NULL;

	int retValue = 0;
	int index = 0;
	int screenScan;
	if(scan<224)
		retValue = 1-(gLineTable[scan+1]-gLineTable[scan]);
	screenScan = gLineTable[scan];

	if ((int)screenScan<0) 
		return 0; // Out of range
	if ((int)screenScan>=Targ.view.iBr.iY) 
		return 0; // Out of range
	
	pd=(unsigned short*)(Targ.screen+screenScan*Targ.scanline_length);
	pdSrc1 = pd;

	if((Pico.video.reg[12]&1))
	{
		ps=data; end=ps+320;		
		do
		{
			if(gColumnStepTable[index]>1)
			{
				*pd = gColorMapTab[*ps];
				pd++;
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);	
				index++;
				ps++;
			}
			else if(gColumnStepTable[index]>0)
			{
				*pd = gColorMapTab[*ps];
				index++;
				ps++;				
			}
			else
			{
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
				ps+=2;
				index+=2;
			}
			pd++;
		}
		while (ps<end);

		
	
	}
	else
	{
		ps=data+32; end=ps+256;
		// Reduce 10 pixels into 7
		do
		{
			if(gNarrowColumnStepTable[index]>1)
			{
				*pd = gColorMapTab[*ps];
				pd++;
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);		
				index++;
			}
			else if(gNarrowColumnStepTable[index]>0)
			{
				*pd = gColorMapTab[*ps];
				ps++;
				index++;
			}
			else
			{
				*pd = ((gColorMapTab[*ps]+gColorMapTab[*(ps+1)])>>1);
				ps+=2;
				index+=2;
			}
			pd++;		
		}
		while (ps<end);  	
	}

	if(scan>0 && screenScan != gLineTable[scan-1]+1)
		{		
			pdSrc2 = pdSrc1-Targ.scanline_length;
			pd = pdSrc1-(Targ.scanline_length>>1);

			for(TInt loop=0;loop<Targ.view.iBr.iX;loop++)
			{
				*pd=((*pdSrc1+*pdSrc2)>>1);
				pd++;
				pdSrc1++;
				pdSrc2++;
			}
			
		}
	
	return retValue;
}
