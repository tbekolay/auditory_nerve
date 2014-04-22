/* This is Version 2 of the public distribution of the code for the auditory
   periphery model of:

        Zilany, M. S. A. and Bruce, I. C. (2006). "Modeling auditory-nerve
        responses for high sound pressure levels in the normal and impaired
        auditory periphery," Journal of the Acoustical Society of
        America 120(3):1446�1466,

   and
 
        Zilany, M. S. A. and Bruce, I. C. (2007). "Representation of the vowel
        /eh/ in normal and impaired auditory nerve fibers: Model predictions of
        responses in cats," Journal of the Acoustical Society of America
        122(1):402�417.

   Please cite the Zilany and Bruce (2006, 2007) papers if you publish any research
   results obtained with this code or any modified versions of this code.

   Note that the only difference between the models presented in the two papers is
   the function for cochlear amplifier (CA) gain versus characteristic frequency (CF),
   given by Eq. (6) in Zilany and Bruce (2006) and by Eq. (1) in Zilany and Bruce (2007).
   Since version 1.1 of the code, the new CA gain versus CF function of Zilany and Bruce
   (2007) is used by default.  If you wish to use the old function, then you will need to
   uncomment line 288 of this code and comment out line 289, and then recompile the code.

   See the file readme.txt for details of compiling and running the model.  
   
   %%% � Ian C. Bruce (ibruce@ieee.org), M. S. Arefeen Zilany, Rasha Ibrahim, June 2006 - December 2007 %%%
   
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mex.h>
#include <time.h>

#include "complex.h"
#include "spikegen.h"
#include "gsl_rng.h"

#ifndef TWOPI
#define TWOPI 6.28318530717959
#endif

#ifndef __max
#define __max(a,b) (((a) > (b))? (a): (b))
#endif

#ifndef __min
#define __min(a,b) (((a) < (b))? (a): (b))
#endif

/* This function is the MEX "wrapper", to pass the input and output variables between the .dll or .mexglx file and Matlab */

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	
	double *px, cf, binwidth, reptime, spont, cohc, cihc;
	int    nrep, pxbins, lp, outsize[2], totalstim;

	double *pxtmp, *cftmp, *nreptmp, *binwidthtmp, *reptimetmp, *cohctmp, *cihctmp, *sponttmp;
	
    double *timeout, *meout, *c1filterout, *c2filterout, *c1vihc, *c2vihc, *ihcout, *synout, *psth;
   
	void   SingleAN(double *, double, int, double, int, double, double, double,
			        double *, double *, double *, double *, double *, double *, double *,
                    double *, double *);
	
	/* Check for proper number of arguments */
	
	if (nrhs != 8) 
	{
		mexErrMsgTxt("zbcatmodel requires 8 input arguments.");
	}; 

	if (nlhs != 9)  
	{
		mexErrMsgTxt("zbcatmodel requires 9 output arguments.");
	};
	
	/* Assign pointers to the inputs */

	pxtmp		= mxGetPr(prhs[0]);
	cftmp		= mxGetPr(prhs[1]);
	nreptmp		= mxGetPr(prhs[2]);
	binwidthtmp	= mxGetPr(prhs[3]);
	reptimetmp	= mxGetPr(prhs[4]);
    cohctmp		= mxGetPr(prhs[5]);
    cihctmp		= mxGetPr(prhs[6]);
	sponttmp	= mxGetPr(prhs[7]);
	
	/* Check with individual input arguments */

	pxbins = mxGetN(prhs[0]);
	if (pxbins==1)
		mexErrMsgTxt("px must be a row vector\n");

    binwidth = binwidthtmp[0];
	if ((binwidth<0.002e-3)|(binwidth>0.01e-3))
	{
		mexErrMsgTxt("binwidth should be between 0.002 and 0.010 ms (100 <= Fs <= 500 kHz).\n");
	}

    if (floor(binwidth*1e6)!=ceil(binwidth*1e6))
	{
		mexErrMsgTxt("binwidth should be an integer number when given in units of microsecond.\n");
	}
   
    
	cf = cftmp[0];

    if (binwidth>0.005e-3)
    {
        if ((cf<80)|(cf>20e3))
        {
            mexPrintf("cf (= %1.1f Hz) must be between 80 Hz and 20 kHz\n",cf);
            mexPrintf("for sampling rates below 200 kHz.\n");
            mexErrMsgTxt("\n");
        }
    }
    else
    {
        if ((cf<80)|(cf>40e3))
        {
            mexPrintf("cf (= %1.1f Hz) must be between 80 Hz and 40 kHz\n",cf);
            mexErrMsgTxt("\n");
        }
    }
	
	nrep = (int)nreptmp[0];
	if (nreptmp[0]!=nrep)
		mexErrMsgTxt("nrep must an integer.\n");
	if (nrep<1)
		mexErrMsgTxt("nrep must be greater that 0.\n");

    cohc = cohctmp[0]; /* impairment in the OHC  */
	if ((cohc<0)|(cohc>1))
	{
		mexPrintf("cohc (= %1.1f) must be between 0 and 1\n",cohc);
		mexErrMsgTxt("\n");
	}

	cihc = cihctmp[0]; /* impairment in the IHC  */
	if ((cihc<0)|(cihc>1))
	{
		mexPrintf("cihc (= %1.1f) must be between 0 and 1\n",cihc);
		mexErrMsgTxt("\n");
	}

	spont = sponttmp[0];  /* spontaneous rate of the fiber */
	if ((spont<0)|(spont>150))
	{
		mexPrintf("spont (= %1.1f) must be between 0 and 150 spikes/s\n",spont);
		mexErrMsgTxt("\n");
	}
	if (spont<0.1)
    {
		mexPrintf("setting spont rate to 0.1 to create effectively low-spont behavior\n");
        spont = 0.1;
    }        
    
	/* Calculate number of samples for total repetition time */

	reptime = reptimetmp[0];
	totalstim = (int) floor((reptime*1e3)/(binwidth*1e3));

	if (totalstim<pxbins)
		mexErrMsgTxt("reptime should be equal to or longer than the stimulus duration.\n");
    
    
    outsize[0] = 1;
	outsize[1] = totalstim;

    px = (double*)mxCalloc(outsize[1],sizeof(double)); 


	/* Put stimulus waveform into pressure waveform */

	for (lp=0; lp<pxbins; lp++)
			px[lp] = pxtmp[lp];
	
	/* Create an array for the return argument */
	
	plhs[0] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[1] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[2] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[3] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[4] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[5] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[6] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[7] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	plhs[8] = mxCreateNumericArray(2, outsize, mxDOUBLE_CLASS, mxREAL);
	
	/* Assign pointers to the outputs */
	
	timeout	= mxGetPr(plhs[0]);
	meout	= mxGetPr(plhs[1]);
	c1filterout	= mxGetPr(plhs[2]);
	c2filterout	= mxGetPr(plhs[3]);
	c1vihc	= mxGetPr(plhs[4]);
	c2vihc	= mxGetPr(plhs[5]);
	ihcout	= mxGetPr(plhs[6]);
	synout  = mxGetPr(plhs[7]);
	psth	= mxGetPr(plhs[8]);
			
	/* run the model */

	mexPrintf("zbcatmodel: Zilany and Bruce (JASA 2006, 2007) Cat Auditory Nerve Model\n");

	SingleAN(px,cf,nrep,binwidth,totalstim,cohc,cihc,spont,
                timeout,meout,c1filterout,c2filterout,c1vihc,c2vihc,ihcout,synout,psth);

 mxFree(px);

}

void SingleAN(double *px, double cf, int nrep, double binwidth, int totalstim,
                double cohc, double cihc, double spont,
              double *timeout, double *meout, double *c1filterout, double *c2filterout,
                double *c1vihc, double *c2vihc, double *ihcout, double *synout, double *psth)
{	
    
    /*variables for middle-ear model */
	double megainmax=41.1405;
    double *mey1, *mey2, *mey3;
	double fp,C,m11,m12,m21,m22,m23,m24,m25,m26,m31,m32,m33,m34,m35,m36;
	
	/*variables for the signal-path, control-path and onward */
	double *c1filterouttmp,*c2filterouttmp,*c1vihctmp,*c2vihctmp,*ihcouttmp,*synouttmp,*tmpgain,*sptime;
	int    *grd;

    double bmplace,centerfreq,CAgain,taubm,ratiowb,bmTaubm,fcohc,TauWBMax,TauWBMin,tauwb;
    double Taumin[1],Taumax[1],bmTaumin[1],bmTaumax[1],ratiobm[1],lasttmpgain,wbgain,ohcasym,ihcasym,delay;
	int    i,n,delaypoint,grdelay[1],bmorder,wborder,nspikes,ipst;
	double wbout1,wbout,ohcnonlinout,ohcout,tmptauc1,tauc1,rsigma,wb_gain;

        
    /* Declarations of the functions used in the program */

	double C1ChirpFilt(double, double,double, int, double, double);
	double C2ChirpFilt(double, double,double, int, double, double);
    double WbGammaTone(double, double, double, int, double, double, int);

    double Get_tauwb(double, double, int, double *, double *);
	double Get_taubm(double, double, double, double *, double *, double *);
    double gain_groupdelay(double, double, double, double, int *);
    double delay_cat(double cf);

    double OhcLowPass(double, double, double, int, double, int);
    double IhcLowPass(double, double, double, int, double, int);
	double Boltzman(double, double, double, double, double);
    double NLafterohc(double, double, double, double);
	double ControlSignal(double, double, double, double, double);

    double NLogarithm(double, double, double);
    double Synapse(double, double, double, double, int);
	int    SpikeGenerator(double *, double, int, double, double *); 

    /* Allocate dynamic memory for the temporary variables */
   
    c1filterouttmp   = (double*)mxCalloc(totalstim,sizeof(double));
    c2filterouttmp   = (double*)mxCalloc(totalstim,sizeof(double));
    c1vihctmp = (double*)mxCalloc(totalstim,sizeof(double));
    c2vihctmp = (double*)mxCalloc(totalstim,sizeof(double));
	ihcouttmp = (double*)mxCalloc(totalstim,sizeof(double));
	synouttmp = (double*)mxCalloc(totalstim,sizeof(double));
    
	mey1 = (double*)mxCalloc(totalstim,sizeof(double));
	mey2 = (double*)mxCalloc(totalstim,sizeof(double));
	mey3 = (double*)mxCalloc(totalstim,sizeof(double));

	grd     = (int*)mxCalloc(totalstim,sizeof(double));
	tmpgain = (double*)mxCalloc(totalstim,sizeof(double));
    sptime  = (double*)mxCalloc((long) ceil(totalstim*binwidth*nrep/0.00075),sizeof(double));
	/** Calculate the location on basilar membrane from CF */	
    
	bmplace = 11.9 * log10(0.80 + cf / 456.0); 
    
	/** Calculate the center frequency for the control-path wideband filter
	    from the location on basilar membrane */
	
	centerfreq = 456.0*(pow(10,(bmplace+1.2)/11.9)-0.80); /* shift the center freq */
    
	/*==================================================================*/
	/*====== Parameters for the CAgain ===========*/
    /* CAgain = 52/2*(tanh(2.2*log10(cf/1e3)+0.15)+1); */ /* CA gain function used in Zilany and Bruce (2006) */
    CAgain = 52/2*(tanh(2.2*log10(cf/600)+0.15)+1);    /* CA gain function used in Zilany and Bruce (2007) */
    if(CAgain<15) CAgain = 15;

    /*====== Parameters for the control-path wideband filter =======*/
	bmorder = 3;
	Get_tauwb(cf,CAgain,bmorder,Taumax,Taumin);
	taubm   = cohc*(Taumax[0]-Taumin[0])+Taumin[0];
	ratiowb = Taumin[0]/Taumax[0];
	/*====== Parameters for the signal-path C1 filter ======*/
	Get_taubm(cf,CAgain,Taumax[0],bmTaumax,bmTaumin,ratiobm);
	bmTaubm  = cohc*(bmTaumax[0]-bmTaumin[0])+bmTaumin[0];
	fcohc    = bmTaumax[0]/bmTaubm;
    /*====== Parameters for the control-path wideband filter =======*/
	wborder  = 3;
    TauWBMax = Taumin[0]+0.2*(Taumax[0]-Taumin[0]);
	TauWBMin = TauWBMax/Taumax[0]*Taumin[0];
    tauwb    = TauWBMax+(bmTaubm-bmTaumax[0])*(TauWBMax-TauWBMin)/(bmTaumax[0]-bmTaumin[0]);
	
	wbgain = gain_groupdelay(binwidth,centerfreq,cf,tauwb,grdelay);
	tmpgain[0]   = wbgain; 
	lasttmpgain  = wbgain;
  	/*===============================================================*/
    /* Nonlinear asymmetry of OHC function and IHC C1 transduction function*/
	ohcasym  = 7.0;    
	ihcasym  = 3.0;
  	/*===============================================================*/
    /* Prewarping and related constants for the middle ear */
     fp = 1e3;  /* prewarping frequency 1 kHz */
     C  = TWOPI*fp/tan(TWOPI/2*fp*binwidth);
     m11 = C/(C + 693.48);                    m12 = (693.48 - C)/C;
	 m21 = 1/(pow(C,2) + 11053*C + 1.163e8);  m22 = -2*pow(C,2) + 2.326e8;     m23 = pow(C,2) - 11053*C + 1.163e8; 
	 m24 = pow(C,2) + 1356.3*C + 7.4417e8;    m25 = -2*pow(C,2) + 14.8834e8;   m26 = pow(C,2) - 1356.3*C + 7.4417e8;
	 m31 = 1/(pow(C,2) + 4620*C + 909059944); m32 = -2*pow(C,2) + 2*909059944; m33 = pow(C,2) - 4620*C + 909059944;
	 m34 = 5.7585e5*C + 7.1665e7;             m35 = 14.333e7;                  m36 = 7.1665e7 - 5.7585e5*C;
	 
  	for (n=0;n<totalstim;n++) /* Start of the loop */
    {    
        if (n==0)  /* Start of the middle-ear filtering section  */
		{
	    	mey1[0]  = m11*px[0];
            mey2[0]  = mey1[0]*m24*m21;
            mey3[0]  = mey2[0]*m34*m31;
            meout[0] = mey3[0]/megainmax ;
        }
            
        else if (n==1)
		{
            mey1[1]  = m11*(-m12*mey1[0] + px[1]       - px[0]);
			mey2[1]  = m21*(-m22*mey2[0] + m24*mey1[1] + m25*mey1[0]);
            mey3[1]  = m31*(-m32*mey3[0] + m34*mey2[1] + m35*mey2[0]);
            meout[1] = mey3[1]/megainmax;
		}
	    else 
		{
            mey1[n]  = m11*(-m12*mey1[n-1]  + px[n]         - px[n-1]);
            mey2[n]  = m21*(-m22*mey2[n-1] - m23*mey2[n-2] + m24*mey1[n] + m25*mey1[n-1] + m26*mey1[n-2]);
            mey3[n]  = m31*(-m32*mey3[n-1] - m33*mey3[n-2] + m34*mey2[n] + m35*mey2[n-1] + m36*mey2[n-2]);
            meout[n] = mey3[n]/megainmax;
		}; 	/* End of the middle-ear filtering section */   
	
        timeout[n] = n*binwidth;
     
		/* Control-path filter */

        wbout1 = WbGammaTone(meout[n],binwidth,centerfreq,n,tauwb,wbgain,wborder);
        wbout  = pow((tauwb/TauWBMax),wborder)*wbout1*10e3*__max(1,cf/5e3);

        ohcnonlinout = Boltzman(wbout,ohcasym,12.0,5.0,5.0); /* pass the control signal through OHC Nonlinear Function */
		ohcout = OhcLowPass(ohcnonlinout,binwidth,600, n,1.0,2);/* lowpass filtering after the OHC nonlinearity */
		   
		tmptauc1 = NLafterohc(ohcout,bmTaumin[0],bmTaumax[0],ohcasym); /* nonlinear function after OHC low-pass filter */
		tauc1    = cohc*(tmptauc1-bmTaumin[0])+bmTaumin[0];  /* time -constant for the signal-path C1 filter */
		rsigma   = 1/tauc1-1/bmTaumax[0]; /* shift of the location of poles of the C1 filter from the initial positions */

		if (1/tauc1<0.0) mexErrMsgTxt("The poles are in the right-half plane; system is unstable.\n");

		tauwb = TauWBMax+(tauc1-bmTaumax[0])*(TauWBMax-TauWBMin)/(bmTaumax[0]-bmTaumin[0]);

	    wb_gain = gain_groupdelay(binwidth,centerfreq,cf,tauwb,grdelay);
		
		grd[n] = grdelay[0]; 

        if ((grd[n]+n)<totalstim)
	         tmpgain[grd[n]+n] = wb_gain;

        if (tmpgain[n] == 0)
			tmpgain[n] = lasttmpgain;	
		
		wbgain      = tmpgain[n];
		lasttmpgain = wbgain;
	 		        
        /*====== Signal-path C1 filter ======*/
         
		 c1filterouttmp[n] = C1ChirpFilt(meout[n], binwidth, cf, n, bmTaumax[0], rsigma); /* C1 filter output */

	 
        /*====== Parallel-path C2 filter ======*/

		 c2filterouttmp[n]  = C2ChirpFilt(meout[n], binwidth, cf, n, bmTaumax[0], 1/ratiobm[0]); /* parallel-filter output*/

	    /*=== Run the inner hair cell (IHC) section: NL function and then lowpass filtering ===*/

         c1vihctmp[n]  = NLogarithm(cihc*c1filterouttmp[n],0.1,ihcasym);
	    
		 c2vihctmp[n] = -NLogarithm(c2filterouttmp[n]*fabs(c2filterouttmp[n])*cf/10*cf/2e3,0.2,1.0); /* C2 transduction output */

	     ihcouttmp[n] = IhcLowPass(c1vihctmp[n]+c2vihctmp[n],binwidth,3800,n,1.0,7);
        
        /*====== Run the synapse model ======*/
		
		 synouttmp[n] = Synapse(ihcouttmp[n],binwidth,cf,spont,n);
  	
   };  /* End of the loop */

	/* Adjust total path delay to all signals after BM */

	delay      = delay_cat(cf);
	delaypoint =__max(0,(int) ceil(delay/binwidth));

	for(i=0;i<delaypoint;i++)
	{
		synout[i] = spont;		
	};
    for(i=delaypoint;i<totalstim;i++)
	{
		c1filterout[i]  = c1filterouttmp[i-delaypoint];
		c2filterout[i]  = c2filterouttmp[i-delaypoint];
		c1vihc[i] = c1vihctmp[i-delaypoint];
		c2vihc[i] = c2vihctmp[i-delaypoint];
		ihcout[i] = ihcouttmp[i-delaypoint];
		synout[i] = synouttmp[i-delaypoint];
	};

    /* Spike Generation */

	nspikes = SpikeGenerator(synout, binwidth, totalstim, nrep, sptime);
	
	for(i = 0; i < nspikes; i++)
	{
		ipst = (int) (fmod(sptime[i],binwidth*totalstim) / binwidth);
        psth[ipst] = psth[ipst] + 1;
	};


/* Freeing dynamic memory allocated earlier */

mxFree(c1filterouttmp);
mxFree(c2filterouttmp);
mxFree(c1vihctmp);
mxFree(c2vihctmp);
mxFree(ihcouttmp);
mxFree(synouttmp);

mxFree(mey1);
mxFree(mey2);
mxFree(mey3);	

mxFree(sptime);
mxFree(grd);
mxFree(tmpgain);

} /* End of the SingleAN function */
/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */
/** Get TauMax, TauMin for the tuning filter. The TauMax is determined by the bandwidth/Q10
    of the tuning filter at low level. The TauMin is determined by the gain change between high
    and low level */

double Get_tauwb(double cf, double CAgain,int order, double *taumax,double *taumin)
{

  double Q10,bw,ratio;
    
  ratio = pow(10,(-CAgain/(20.0*order)));       /* ratio of TauMin/TauMax according to the gain, order */
  
  /* Q10 = pow(10,0.4708*log10(cf/1e3)+0.5469); */ /* 75th percentile */
    Q10 = pow(10,0.4708*log10(cf/1e3)+0.4664); /* 50th percentile */
  /* Q10 = pow(10,0.4708*log10(cf/1e3)+0.3934); */ /* 25th percentile */
  
  bw     = cf/Q10;
  taumax[0] = 2.0/(TWOPI*bw);
   
  taumin[0]   = taumax[0]*ratio;
  
  return 0;
}
/* -------------------------------------------------------------------------------------------- */
double Get_taubm(double cf, double CAgain, double taumax,double *bmTaumax,double *bmTaumin, double *ratio)
{
  double factor,bwfactor;
    
  bwfactor = 0.7;
  factor   = 2.5;

  ratio[0]  = pow(10,(-CAgain/(20.0*factor))); 

  bmTaumax[0] = taumax/bwfactor;
  bmTaumin[0] = bmTaumax[0]*ratio[0];     
  return 0;
}
/* -------------------------------------------------------------------------------------------- */
/** Pass the signal through the signal-path C1 Tenth Order Nonlinear Chirp-Gammatone Filter */

double C1ChirpFilt(double x, double binwidth,double cf, int n, double taumax, double rsigma)
{
    static double C1gain_norm, C1initphase; 
    static double C1input[12][4], C1output[12][4];

    double ipw, ipb, rpa, pzero, rzero;
	double sigma0,fs_bilinear,CF,norm_gain,phase,c1filterout;
	int i,r,order_of_pole,half_order_pole,order_of_zero;
	double temp, dy, preal, pimg;

	COMPLEX p[11]; 
	
	/* Defining initial locations of the poles and zeros */
	/*======== setup the locations of poles and zeros =======*/
	  sigma0 = 1/taumax;
	  ipw    = 1.01*cf*TWOPI-50;
	  ipb    = 0.2343*TWOPI*cf-1104;
	  rpa    = pow(10, log10(cf)*0.9 + 0.55)+ 2000;
	  pzero  = pow(10,log10(cf)*0.7+1.6)+500;

	/*===============================================================*/     
         
     order_of_pole    = 10;             
     half_order_pole  = order_of_pole/2;
     order_of_zero    = half_order_pole;

	 fs_bilinear = TWOPI*cf/tan(TWOPI*cf*binwidth/2);
     rzero       = -pzero;
	 CF          = TWOPI*cf;
   
   if (n==0)
   {		  
	p[1].x = -sigma0;     

    p[1].y = ipw;

	p[5].x = p[1].x - rpa; p[5].y = p[1].y - ipb;

    p[3].x = (p[1].x + p[5].x) * 0.5; p[3].y = (p[1].y + p[5].y) * 0.5;

    p[2]   = compconj(p[1]);    p[4] = compconj(p[3]); p[6] = compconj(p[5]);

    p[7]   = p[1]; p[8] = p[2]; p[9] = p[5]; p[10]= p[6];

	   C1initphase = 0.0;
       for (i=1;i<=half_order_pole;i++)          
	   {
           preal     = p[i*2-1].x;
		   pimg      = p[i*2-1].y;
	       C1initphase = C1initphase + atan(CF/(-rzero))-atan((CF-pimg)/(-preal))-atan((CF+pimg)/(-preal));
	   };

	/*===================== Initialize C1input & C1output =====================*/

      for (i=1;i<=(half_order_pole+1);i++)          
      {
		   C1input[i][3] = 0; 
		   C1input[i][2] = 0; 
		   C1input[i][1] = 0;
		   C1output[i][3] = 0; 
		   C1output[i][2] = 0; 
		   C1output[i][1] = 0;
      }

	/*===================== normalize the gain =====================*/
    
      C1gain_norm = 1.0;
      for (r=1; r<=order_of_pole; r++)
		   C1gain_norm = C1gain_norm*(pow((CF - p[r].y),2) + p[r].x*p[r].x);
      
   };
     
    norm_gain= sqrt(C1gain_norm)/pow(sqrt(CF*CF+rzero*rzero),order_of_zero);
	
	p[1].x = -sigma0 - rsigma;

	if (p[1].x>0.0) mexErrMsgTxt("The system becomes unstable.\n");
	
	p[1].y = ipw;

	p[5].x = p[1].x - rpa; p[5].y = p[1].y - ipb;

    p[3].x = (p[1].x + p[5].x) * 0.5; p[3].y = (p[1].y + p[5].y) * 0.5;

    p[2] = compconj(p[1]); p[4] = compconj(p[3]); p[6] = compconj(p[5]);

    p[7] = p[1]; p[8] = p[2]; p[9] = p[5]; p[10]= p[6];

    phase = 0.0;
    for (i=1;i<=half_order_pole;i++)          
    {
           preal = p[i*2-1].x;
		   pimg  = p[i*2-1].y;
	       phase = phase-atan((CF-pimg)/(-preal))-atan((CF+pimg)/(-preal));
	};

	rzero = -CF/tan((C1initphase-phase)/order_of_zero);

    if (rzero>0.0) mexErrMsgTxt("The zeros are in the right-half plane.\n");
	 
   /*%==================================================  */
	/*each loop below is for a pair of poles and one zero */
   /*%      time loop begins here                         */
   /*%==================================================  */
 
       C1input[1][3]=C1input[1][2]; 
	   C1input[1][2]=C1input[1][1]; 
	   C1input[1][1]= x;

       for (i=1;i<=half_order_pole;i++)          
       {
           preal = p[i*2-1].x;
		   pimg  = p[i*2-1].y;
		  	   
           temp  = pow((fs_bilinear-preal),2)+ pow(pimg,2);
		   

           /*dy = (input[i][1] + (1-(fs_bilinear+rzero)/(fs_bilinear-rzero))*input[i][2]
                                 - (fs_bilinear+rzero)/(fs_bilinear-rzero)*input[i][3] );
           dy = dy+2*output[i][1]*(fs_bilinear*fs_bilinear-preal*preal-pimg*pimg);

           dy = dy-output[i][2]*((fs_bilinear+preal)*(fs_bilinear+preal)+pimg*pimg);*/
		   
	       dy = C1input[i][1]*(fs_bilinear-rzero) - 2*rzero*C1input[i][2] - (fs_bilinear+rzero)*C1input[i][3]
                 +2*C1output[i][1]*(fs_bilinear*fs_bilinear-preal*preal-pimg*pimg)
			     -C1output[i][2]*((fs_bilinear+preal)*(fs_bilinear+preal)+pimg*pimg);

		   dy = dy/temp;

		   C1input[i+1][3] = C1output[i][2]; 
		   C1input[i+1][2] = C1output[i][1]; 
		   C1input[i+1][1] = dy;

		   C1output[i][2] = C1output[i][1]; 
		   C1output[i][1] = dy;
       }

	   dy = C1output[half_order_pole][1]*norm_gain;  /* don't forget the gain term */
	   c1filterout= dy/4.0;   /* signal path output is divided by 4 to give correct C1 filter gain */
	                   
     return (c1filterout);
}  

/* -------------------------------------------------------------------------------------------- */
/** Parallelpath C2 filter: same as the signal-path C1 filter with the OHC completely impaired */

double C2ChirpFilt(double xx, double binwidth,double cf, int n, double taumax, double fcohc)
{
	static double C2gain_norm, C2initphase;
    static double C2input[12][4];  static double C2output[12][4];
   
	double ipw, ipb, rpa, pzero, rzero;

	double sigma0,fs_bilinear,CF,norm_gain,phase,c2filterout;
	int    i,r,order_of_pole,half_order_pole,order_of_zero;
	double temp, dy, preal, pimg;

	COMPLEX p[11]; 

	
    /*================ setup the locations of poles and zeros =======*/

	  sigma0 = 1/taumax;
	  ipw    = 1.01*cf*TWOPI-50;
      ipb    = 0.2343*TWOPI*cf-1104;
	  rpa    = pow(10, log10(cf)*0.9 + 0.55)+ 2000;
	  pzero  = pow(10,log10(cf)*0.7+1.6)+500;
	/*===============================================================*/     
         
     order_of_pole    = 10;             
     half_order_pole  = order_of_pole/2;
     order_of_zero    = half_order_pole;

	 fs_bilinear = TWOPI*cf/tan(TWOPI*cf*binwidth/2);
     rzero       = -pzero;
	 CF          = TWOPI*cf;
   	    
    if (n==0)
    {		  
	p[1].x = -sigma0;     

    p[1].y = ipw;

	p[5].x = p[1].x - rpa; p[5].y = p[1].y - ipb;

    p[3].x = (p[1].x + p[5].x) * 0.5; p[3].y = (p[1].y + p[5].y) * 0.5;

    p[2] = compconj(p[1]); p[4] = compconj(p[3]); p[6] = compconj(p[5]);

    p[7] = p[1]; p[8] = p[2]; p[9] = p[5]; p[10]= p[6];

	   C2initphase = 0.0;
       for (i=1;i<=half_order_pole;i++)         
	   {
           preal     = p[i*2-1].x;
		   pimg      = p[i*2-1].y;
	       C2initphase = C2initphase + atan(CF/(-rzero))-atan((CF-pimg)/(-preal))-atan((CF+pimg)/(-preal));
	   };

	/*===================== Initialize C2input & C2output =====================*/

      for (i=1;i<=(half_order_pole+1);i++)          
      {
		   C2input[i][3] = 0; 
		   C2input[i][2] = 0; 
		   C2input[i][1] = 0;
		   C2output[i][3] = 0; 
		   C2output[i][2] = 0; 
		   C2output[i][1] = 0;
      }
    
    /*===================== normalize the gain =====================*/
    
     C2gain_norm = 1.0;
     for (r=1; r<=order_of_pole; r++)
		   C2gain_norm = C2gain_norm*(pow((CF - p[r].y),2) + p[r].x*p[r].x);
    };
     
    norm_gain= sqrt(C2gain_norm)/pow(sqrt(CF*CF+rzero*rzero),order_of_zero);
    
	p[1].x = -sigma0*fcohc;

	if (p[1].x>0.0) mexErrMsgTxt("The system becomes unstable.\n");
	
	p[1].y = ipw;

	p[5].x = p[1].x - rpa; p[5].y = p[1].y - ipb;

    p[3].x = (p[1].x + p[5].x) * 0.5; p[3].y = (p[1].y + p[5].y) * 0.5;

    p[2] = compconj(p[1]); p[4] = compconj(p[3]); p[6] = compconj(p[5]);

    p[7] = p[1]; p[8] = p[2]; p[9] = p[5]; p[10]= p[6];

    phase = 0.0;
    for (i=1;i<=half_order_pole;i++)          
    {
           preal = p[i*2-1].x;
		   pimg  = p[i*2-1].y;
	       phase = phase-atan((CF-pimg)/(-preal))-atan((CF+pimg)/(-preal));
	};

	rzero = -CF/tan((C2initphase-phase)/order_of_zero);	
    if (rzero>0.0) mexErrMsgTxt("The zeros are in the right-hand plane.\n");
   /*%==================================================  */
   /*%      time loop begins here                         */
   /*%==================================================  */

       C2input[1][3]=C2input[1][2]; 
	   C2input[1][2]=C2input[1][1]; 
	   C2input[1][1]= xx;

      for (i=1;i<=half_order_pole;i++)          
      {
           preal = p[i*2-1].x;
		   pimg  = p[i*2-1].y;
		  	   
           temp  = pow((fs_bilinear-preal),2)+ pow(pimg,2);
		   
           /*dy = (input[i][1] + (1-(fs_bilinear+rzero)/(fs_bilinear-rzero))*input[i][2]
                                 - (fs_bilinear+rzero)/(fs_bilinear-rzero)*input[i][3] );
           dy = dy+2*output[i][1]*(fs_bilinear*fs_bilinear-preal*preal-pimg*pimg);

           dy = dy-output[i][2]*((fs_bilinear+preal)*(fs_bilinear+preal)+pimg*pimg);*/
		   
	      dy = C2input[i][1]*(fs_bilinear-rzero) - 2*rzero*C2input[i][2] - (fs_bilinear+rzero)*C2input[i][3]
                 +2*C2output[i][1]*(fs_bilinear*fs_bilinear-preal*preal-pimg*pimg)
			     -C2output[i][2]*((fs_bilinear+preal)*(fs_bilinear+preal)+pimg*pimg);

		   dy = dy/temp;

		   C2input[i+1][3] = C2output[i][2]; 
		   C2input[i+1][2] = C2output[i][1]; 
		   C2input[i+1][1] = dy;

		   C2output[i][2] = C2output[i][1]; 
		   C2output[i][1] = dy;

       };

	  dy = C2output[half_order_pole][1]*norm_gain;
	  c2filterout= dy/4.0;
	  
	  return (c2filterout); 
}   

/* -------------------------------------------------------------------------------------------- */
/** Pass the signal through the Control path Third Order Nonlinear Gammatone Filter */

double WbGammaTone(double x,double binwidth,double centerfreq, int n, double tau,double gain,int order)
{
  static double wbphase;
  static COMPLEX wbgtf[4], wbgtfl[4];

  double delta_phase,dtmp,c1LP,c2LP,out;
  int i,j;
  
  if (n==0)
  {
      wbphase = 0;
      for(i=0; i<=order;i++)
      {
            wbgtfl[i] = compmult(0,compexp(0));
            wbgtf[i]  = compmult(0,compexp(0));
      }
  }
  
  delta_phase = -TWOPI*centerfreq*binwidth;
  wbphase += delta_phase;
  
  dtmp = tau*2.0/binwidth;
  c1LP = (dtmp-1)/(dtmp+1);
  c2LP = 1.0/(dtmp+1);
  wbgtf[0] = compmult(x,compexp(wbphase));                 /* FREQUENCY SHIFT */
  
  for(j = 1; j <= order; j++)                              /* IIR Bilinear transformation LPF */
  wbgtf[j] = comp2sum(compmult(c2LP*gain,comp2sum(wbgtf[j-1],wbgtfl[j-1])),
      compmult(c1LP,wbgtfl[j]));
  out = REAL(compprod(compexp(-wbphase), wbgtf[order])); /* FREQ SHIFT BACK UP */
  
  for(i=0; i<=order;i++) wbgtfl[i] = wbgtf[i];
  return(out);
}

/* -------------------------------------------------------------------------------------------- */
/** Calculate the gain and group delay for the Control path Filter */

double gain_groupdelay(double binwidth,double centerfreq, double cf, double tau,int *grdelay)
{ 
  double tmpcos,dtmp2,c1LP,c2LP,tmp1,tmp2,wb_gain;

  tmpcos = cos(TWOPI*(centerfreq-cf)*binwidth);
  dtmp2 = tau*2.0/binwidth;
  c1LP = (dtmp2-1)/(dtmp2+1);
  c2LP = 1.0/(dtmp2+1);
  tmp1 = 1+c1LP*c1LP-2*c1LP*tmpcos;
  tmp2 = 2*c2LP*c2LP*(1+tmpcos);
  
  wb_gain = pow(tmp1/tmp2, 1.0/2.0);
  
  grdelay[0] = (int)floor((0.5-(c1LP*c1LP-c1LP*tmpcos)/(1+c1LP*c1LP-2*c1LP*tmpcos)));

  return(wb_gain);
}
/* -------------------------------------------------------------------------------------------- */
/** Calculate the delay (basilar membrane, synapse, etc. for cat) */

double delay_cat(double cf)
{
  /* DELAY THE WAVEFORM (delay buf1, tauf, ihc for display purposes)  */
  /* Note: Latency vs. CF for click responses is available for Cat only (not human) */
  /* Use original fit for Tl (latency vs. CF in msec) from Carney & Yin '88
     and then correct by .75 cycles to go from PEAK delay to ONSET delay */
	/* from Carney and Yin '88 */
  double A0,A1,x,delay;

  A0    = 3.0;  
  A1    = 12.5;
  x     = 11.9 * log10(0.80 + cf / 456.0);      /* cat mapping */
  delay = A0 * exp( -x/A1 ) * 1e-3;
  
  return(delay);
}
/* -------------------------------------------------------------------------------------------- */
/* Get the output of the OHC Nonlinear Function (Boltzman Function) */

double Boltzman(double x, double asym, double s0, double s1, double x1)
  {
	double shift,x0,out1,out;

    shift = 1.0/(1.0+asym);  /* asym is the ratio of positive Max to negative Max*/
    x0    = s0*log((1.0/shift-1)/(1+exp(x1/s1)));
	    
    out1 = 1.0/(1.0+exp(-(x-x0)/s0)*(1.0+exp(-(x-x1)/s1)))-shift;
	out = out1/(1-shift);

    return(out);
  }  /* output of the nonlinear function, the output is normalized with maximum value of 1 */
  
/* -------------------------------------------------------------------------------------------- */
/* Get the output of the OHC Low Pass Filter in the Control path */

double OhcLowPass(double x,double binwidth,double Fc, int n,double gain,int order)
{
  static double ohc[4],ohcl[4];

  double c,c1LP,c2LP;
  int i,j;

  if (n==0)
  {
      for(i=0; i<(order+1);i++)
      {
          ohc[i] = 0;
          ohcl[i] = 0;
      }
  }    
  
  c = 2.0/binwidth;
  c1LP = ( c - TWOPI*Fc ) / ( c + TWOPI*Fc );
  c2LP = TWOPI*Fc / (TWOPI*Fc + c);
  
  ohc[0] = x*gain;
  for(i=0; i<order;i++)
    ohc[i+1] = c1LP*ohcl[i+1] + c2LP*(ohc[i]+ohcl[i]);
  for(j=0; j<=order;j++) ohcl[j] = ohc[j];
  return(ohc[order]);
}
/* -------------------------------------------------------------------------------------------- */
/* Get the output of the IHC Low Pass Filter  */

double IhcLowPass(double x,double binwidth,double Fc, int n,double gain,int order)
{
  static double ihc[8],ihcl[8];

  double C,c1LP,c2LP;
  int i,j;

  if (n==0)
  {
      for(i=0; i<(order+1);i++)
      {
          ihc[i] = 0;
          ihcl[i] = 0;
      }
  }    
  
  
  C = 2.0/binwidth;
  c1LP = ( C - TWOPI*Fc ) / ( C + TWOPI*Fc );
  c2LP = TWOPI*Fc / (TWOPI*Fc + C);
  
  ihc[0] = x*gain;
  for(i=0; i<order;i++)
    ihc[i+1] = c1LP*ihcl[i+1] + c2LP*(ihc[i]+ihcl[i]);
  for(j=0; j<=order;j++) ihcl[j] = ihc[j];
  return(ihc[order]);
}
/* -------------------------------------------------------------------------------------------- */
/* Get the output of the Control path using Nonlinear Function after OHC */

double NLafterohc(double x,double taumin, double taumax, double asym)
{    
	double R,dc,R1,s0,x1,out,minR;

	minR = 0.05;
    R  = taumin/taumax;
    
	if(R<minR) minR = 0.5*R;
    else       minR = minR;
    
    dc = (asym-1)/(asym+1.0)/2.0-minR;
    R1 = R-minR;

    /* This is for new nonlinearity */
    s0 = -dc/log(R1/(1-minR));
	
    x1  = fabs(x);
    out = taumax*(minR+(1.0-minR)*exp(-x1/s0));
	if (out<taumin) out = taumin; if (out>taumax) out = taumax;
    return(out);
}
/* -------------------------------------------------------------------------------------------- */
/* Get the output of the IHC Nonlinear Function (Logarithmic Transduction Functions) */

double NLogarithm(double x, double slope, double asym)
{
	double corner,strength,xx,splx,asym_t;

    corner    = 80;
    strength  = 20.0e6/pow(10,corner/20); 
            
    xx = log(1.0+strength*fabs(x))*slope;
    if(x<0)
	{
		splx   = 20*log10(-x/20e-6);
		asym_t = asym-(asym-1)/(1+exp(splx/5.0));
		xx = -1/asym_t*xx;
	};
    
    return(xx);
}

/* -------------------------------------------------------------------------------------------- */
/*  Synapse model: if the time resolution is not small enough, the concentration of
   the immediate pool could be as low as negative, at this time there is an alert message
   print out and the concentration is set at saturated level  */

double Synapse(double x, double binwidth, double cf, double spont, int n)
{
	static double synstrength,synslope,CI,CL,PG,CG,VL,PL,VI;

	double cf_factor,PImax,kslope,Ass,Asp,TauR,TauST,Ar_Ast,PTS,Aon,AR,AST,Prest,gamma1,gamma2,k1,k2;
	double VI0,VI1,alpha,beta,theta1,theta2,theta3,vsat,tmpst,tmp,PPI,CIlast,temp,out;
    double cfsat,cfslope,cfconst;

    if (n==0)
	{
/*        cf_factor = __min(1e3,pow(10,0.29*cf/1e3 + 0.4));
 */
       if (spont >= 50)
           cf_factor = __min(1e3,pow(10,0.29*cf/1e3 + 0.4));
       else
       {
           cfslope = pow(spont,0.19)*pow(10,-0.87);
           cfconst = 0.1*pow(log10(spont),2)+0.56*log10(spont)-0.84;
           cfsat = pow(10,cfslope*8965.5/1e3 + cfconst);    /*find saturation at saturation freq: 8965.5 Hz*/
           cf_factor = __min(cfsat,pow(10,cfslope*cf/1e3 + cfconst));
       };                                       /*added by Tim Zeyl June 14 2006*/
		
	   PImax  = 0.6;                /* PI2 : Maximum of the PI(PI at steady state) */
       kslope = (1+50.0)/(5+50.0)*cf_factor*20.0*PImax;           
    	   
       Ass    = 350;                /* Steady State Firing Rate eq.10 */
       Asp    = spont;              /* Spontaneous Firing Rate  eq.10 */
       TauR   = 2e-3;               /* Rapid Time Constant eq.10 */
       TauST  = 60e-3;              /* Short Time Constant eq.10 */
       Ar_Ast = 6;                  /* Ratio of Ar/Ast */
       PTS    = 1.0+9.0*50.0/(9.0+50.0);    /* Peak to Steady State Ratio, characteristic of PSTH */
   
       /* now get the other parameters */
       Aon    = PTS*Ass;                          /* Onset rate = Ass+Ar+Ast eq.10 */
       AR     = (Aon-Ass)*Ar_Ast/(1+Ar_Ast);      /* Rapid component magnitude: eq.10 */
       AST    = Aon-Ass-AR;                       /* Short time component: eq.10 */
       Prest  = PImax/Aon*Asp;                    /* eq.A15 */
       CG  = (Asp*(Aon-Asp))/(Aon*Prest*(1-Asp/Ass));    /* eq.A16 */
       gamma1 = CG/Asp;                           /* eq.A19 */
       gamma2 = CG/Ass;                           /* eq.A20 */
       k1     = -1/TauR;                          /* eq.8 & eq.10 */
       k2     = -1/TauST;                         /* eq.8 & eq.10 */
               /* eq.A21 & eq.A22 */
       VI0    = (1-PImax/Prest)/(gamma1*(AR*(k1-k2)/CG/PImax+k2/Prest/gamma1-k2/PImax/gamma2));
       VI1    = (1-PImax/Prest)/(gamma1*(AST*(k2-k1)/CG/PImax+k1/Prest/gamma1-k1/PImax/gamma2));
       VI  = (VI0+VI1)/2;
       alpha  = gamma2/k1/k2;       /* eq.A23,eq.A24 or eq.7 */
       beta   = -(k1+k2)*alpha;     /* eq.A23 or eq.7 */
       theta1 = alpha*PImax/VI; 
       theta2 = VI/PImax;
       theta3 = gamma2-1/PImax;
  
       PL  = ((beta-theta2*theta3)/theta1-1)*PImax;  /* eq.4' */
       PG  = 1/(theta3-1/PL);                        /* eq.5' */
       VL  = theta1*PL*PG;                           /* eq.3' */
       CI  = Asp/Prest;                              /* CI at rest, from eq.A3,eq.A12 */
       CL  = CI*(Prest+PL)/PL;                       /* CL at rest, from eq.1 */
   	
       if(kslope>=0)  vsat    = kslope+Prest;                
       tmpst  = log(2)*vsat/Prest;
       if(tmpst<400) synstrength = log(exp(tmpst)-1);
       else synstrength = tmpst;
       synslope = Prest/log(2)*synstrength;

    	if(spont<0) spont = 50;
   };
    
	     tmp = synstrength*x;
         if(tmp<400) tmp = log(1+exp(tmp));
         PPI = synslope/synstrength*tmp;
	
         CIlast = CI;
         CI = CI + (binwidth/VI)*(-PPI*CI + PL*(CL-CI));
         CL = CL + (binwidth/VL)*(-PL*(CL - CIlast) + PG*(CG - CL));

         if(CI<0)
		 {
           temp = 1/PG+1/PL+1/PPI;
           CI = CG/(PPI*temp);
           CL = CI*(PPI+PL)/PL;
         };

  out= CI*PPI;
  return(out);
}

