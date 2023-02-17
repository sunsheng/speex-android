/* Copyright (C) 2002-2006 Jean-Marc Valin
   File: speexenc.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#include <stdio.h>
#include <unistd.h>


#include "getopt_win.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "speex/speex.h"
#include "speex/speex_header.h"




#if !defined(__LITTLE_ENDIAN__) && ( defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) )
#define le_short(s) ((short) ((unsigned short) (s) << 8) | ((unsigned short) (s) >> 8))
#define be_short(s) ((short) (s))
#else
#define le_short(s) ((short) (s))
#define be_short(s) ((short) ((unsigned short) (s) << 8) | ((unsigned short) (s) >> 8))
#endif


void comment_init(char **comments, int* length, char *vendor_string);
void comment_add(char **comments, int* length, char *tag, char *val);



#define MAX_FRAME_SIZE 2000
#define MAX_FRAME_BYTES 2000

/* Convert input audio bits, endians and channels */
static int read_samples(FILE *fin,int frame_size, int bits, int channels, int lsb, short * input, char *buff, spx_int32_t *size)
{
   unsigned char in[MAX_FRAME_BYTES*2];
   int i;
   short *s;
   int nb_read;
   size_t to_read;

   if (size && *size<=0)
   {
      return 0;
   }

   to_read = bits/8*channels*frame_size;

   /*Read input audio*/
   if (size)
   {
      if (*size >= to_read)
      {
         *size -= to_read;
      }
      else
      {
         to_read = *size;
         *size = 0;
      }
   }

   if (buff)
   {
      for (i=0;i<12;i++)
         in[i]=buff[i];
      nb_read = fread(in+12,1,to_read-12,fin) + 12;
      if (size)
         *size += 12;
   } else {
      nb_read = fread(in,1,to_read,fin);
   }

   nb_read /= bits/8*channels;

   /*fprintf (stderr, "%d\n", nb_read);*/
   if (nb_read==0)
      return 0;

   s=(short*)in;
   if(bits==8)
   {
      /* Convert 8->16 bits */
      for(i=frame_size*channels-1;i>=0;i--)
      {
         s[i]=(in[i]<<8)^0x8000;
      }
   } else
   {
      /* convert to our endian format */
      for(i=0;i<frame_size*channels;i++)
      {
         if(lsb)
            s[i]=le_short(s[i]);
         else
            s[i]=be_short(s[i]);
      }
   }

   /* FIXME: This is probably redundent now */
   /* copy to float input buffer */
   for (i=0;i<frame_size*channels;i++)
   {
      input[i]=(short)s[i];
   }

   for (i=nb_read*channels;i<frame_size*channels;i++)
   {
      input[i]=0;
   }

   return nb_read;
}


void version()
{
   const char* speex_version;
   speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
   printf ("speexenc (Speex encoder) version %s (compiled " __DATE__ ")\n", speex_version);
   printf ("Copyright (C) 2002-2006 Jean-Marc Valin\n");
}

void version_short()
{
   const char* speex_version;
   speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
   printf ("speexenc version %s\n", speex_version);
   printf ("Copyright (C) 2002-2006 Jean-Marc Valin\n");
}

void usage()
{
   printf ("Usage: speexenc [options] input_file output_file\n");
   printf ("\n");
   printf ("Encodes input_file using Speex. It can read the WAV or raw files.\n");
   printf ("\n");
   printf ("input_file can be:\n");
   printf ("  filename.wav      wav file\n");
   printf ("  filename.*        Raw PCM file (any extension other than .wav)\n");
   printf ("  -                 stdin\n");
   printf ("\n");
   printf ("output_file can be:\n");
   printf ("  filename.spx      Speex file\n");
   printf ("  -                 stdout\n");
   printf ("\n");
   printf ("Options:\n");
   printf (" -n, --narrowband   Narrowband (8 kHz) input file\n");
   printf (" -w, --wideband     Wideband (16 kHz) input file\n");
   printf (" -u, --ultra-wideband \"Ultra-wideband\" (32 kHz) input file\n");
   printf (" --quality n        Encoding quality (0-10), default 8\n");
   printf (" --bitrate n        Encoding bit-rate (use bit-rate n or lower)\n");
   printf (" --vbr              Enable variable bit-rate (VBR)\n");
   printf (" --vbr-max-bitrate  Set max VBR bit-rate allowed\n");
   printf (" --abr rate         Enable average bit-rate (ABR) at rate bps\n");
   printf (" --vad              Enable voice activity detection (VAD)\n");
   printf (" --dtx              Enable file-based discontinuous transmission (DTX)\n");
   printf (" --comp n           Set encoding complexity (0-10), default 3\n");
   printf (" --nframes n        Number of frames per Ogg packet (1-10), default 1\n");
   printf (" --no-highpass      Disable the encoder's built-in high-pass filter\n");
   printf (" --skeleton         Outputs ogg skeleton metadata (may cause incompatibilities)\n");
   printf (" --comment          Add the given string as an extra comment. This may be\n");
   printf ("                     used multiple times\n");
   printf (" --author           Author of this track\n");
   printf (" --title            Title for this track\n");
   printf (" -h, --help         This help\n");
   printf (" -v, --version      Version information\n");
   printf (" -V                 Verbose mode (show bit-rate)\n");
   printf (" --print-rate       Print the bitrate for each frame to standard output\n");
   printf ("Raw input options:\n");
   printf (" --rate n           Sampling rate for raw input\n");
   printf (" --stereo           Consider raw input as stereo\n");
   printf (" --le               Raw input is little-endian\n");
   printf (" --be               Raw input is big-endian\n");
   printf (" --8bit             Raw input is 8-bit unsigned\n");
   printf (" --16bit            Raw input is 16-bit signed\n");
   printf ("Default raw PCM input is 16-bit, little-endian, mono\n");
   printf ("\n");
   printf ("More information is available from the Speex site: http://www.speex.org\n");
   printf ("\n");
   printf ("Please report bugs to the mailing list `speex-dev@xiph.org'.\n");
}

#include <time.h>

int main(int argc, char **argv)
{
   clock_t start, end;
   double cpu_time_use;
   int nb_samples, total_samples=0, nb_encoded;
   int c;
   int option_index = 0;
   char *inFile, *outFile;
   FILE *fin, *fout;
   short input[MAX_FRAME_SIZE];
   spx_int32_t frame_size;
   int quiet=0;
   spx_int32_t vbr_enabled=0;
   spx_int32_t vbr_max=0;
   int abr_enabled=0;
   spx_int32_t vad_enabled=0;
   spx_int32_t dtx_enabled=0;
   int nbBytes;
   const SpeexMode *mode=NULL;
   int modeID = -1;
   void *st;
   SpeexBits bits;
   char cbits[MAX_FRAME_BYTES];
   int with_skeleton = 0;
   struct option long_options[] =
   {
      {"wideband", no_argument, NULL, 0},
      {"ultra-wideband", no_argument, NULL, 0},
      {"narrowband", no_argument, NULL, 0},
      {"vbr", no_argument, NULL, 0},
      {"vbr-max-bitrate", required_argument, NULL, 0},
      {"abr", required_argument, NULL, 0},
      {"vad", no_argument, NULL, 0},
      {"dtx", no_argument, NULL, 0},
      {"quality", required_argument, NULL, 0},
      {"bitrate", required_argument, NULL, 0},
      {"nframes", required_argument, NULL, 0},
      {"comp", required_argument, NULL, 0},
#ifdef USE_SPEEXDSP
      {"denoise", no_argument, NULL, 0},
      {"agc", no_argument, NULL, 0},
#endif
      {"no-highpass", no_argument, NULL, 0},
      {"skeleton",no_argument,NULL, 0},
      {"help", no_argument, NULL, 0},
      {"quiet", no_argument, NULL, 0},
      {"le", no_argument, NULL, 0},
      {"be", no_argument, NULL, 0},
      {"8bit", no_argument, NULL, 0},
      {"16bit", no_argument, NULL, 0},
      {"stereo", no_argument, NULL, 0},
      {"rate", required_argument, NULL, 0},
      {"version", no_argument, NULL, 0},
      {"version-short", no_argument, NULL, 0},
      {"comment", required_argument, NULL, 0},
      {"author", required_argument, NULL, 0},
      {"title", required_argument, NULL, 0},
      {"print-rate", no_argument, NULL, 0},
      {0, 0, 0, 0}
   };
   int print_bitrate=0;
   spx_int32_t rate=0;
   spx_int32_t size;
   int chan=1;
   int fmt=16;
   spx_int32_t quality=-1;
   float vbr_quality=-1;
   int lsb=1;
   int bytes_written=0, ret, result;
   int id=-1;
   SpeexHeader header;
   int nframes=1;
   spx_int32_t complexity=3;
   const char* speex_version;
   char vendor_string[64];
   // char *comments;
   int comments_length;
   int close_in=0, close_out=0;
   int eos=0;
   spx_int32_t bitrate=0;
   double cumul_bits=0, enc_frames=0;
   char first_bytes[12];

   spx_int32_t tmp;
#ifdef USE_SPEEXDSP
   SpeexPreprocessState *preprocess = NULL;
   int denoise_enabled=0, agc_enabled=0;
#endif
   int highpass_enabled=1;
   int output_rate=0;
   spx_int32_t lookahead = 0;

   speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
   snprintf(vendor_string, sizeof(vendor_string), "Encoded with Speex %s", speex_version);

   // comment_init(&comments, &comments_length, vendor_string);

   /*Process command-line options*/
   while(1)
   {
      c = getopt_long (argc, argv, "nwuhvV",
                       long_options, &option_index);
      if (c==-1)
         break;

      switch(c)
      {
      case 0:
         if (strcmp(long_options[option_index].name,"narrowband")==0)
         {
            modeID = SPEEX_MODEID_NB;
         } else if (strcmp(long_options[option_index].name,"wideband")==0)
         {
            modeID = SPEEX_MODEID_WB;
         } else if (strcmp(long_options[option_index].name,"ultra-wideband")==0)
         {
            modeID = SPEEX_MODEID_UWB;
         } else if (strcmp(long_options[option_index].name,"vbr")==0)
         {
            vbr_enabled=1;
         } else if (strcmp(long_options[option_index].name,"vbr-max-bitrate")==0)
         {
            vbr_max=atoi(optarg);
            if (vbr_max<1)
            {
               fprintf (stderr, "Invalid VBR max bit-rate value: %d\n", vbr_max);
               exit(1);
            }
         } else if (strcmp(long_options[option_index].name,"abr")==0)
         {
            abr_enabled=atoi(optarg);
            if (!abr_enabled)
            {
               fprintf (stderr, "Invalid ABR value: %d\n", abr_enabled);
               exit(1);
            }
         } else if (strcmp(long_options[option_index].name,"vad")==0)
         {
            vad_enabled=1;
         } else if (strcmp(long_options[option_index].name,"dtx")==0)
         {
            dtx_enabled=1;
         } else if (strcmp(long_options[option_index].name,"quality")==0)
         {
            quality = atoi (optarg);
            vbr_quality=atof(optarg);
         } else if (strcmp(long_options[option_index].name,"bitrate")==0)
         {
            bitrate = atoi (optarg);
         } else if (strcmp(long_options[option_index].name,"nframes")==0)
         {
            nframes = atoi (optarg);
            if (nframes<1)
               nframes=1;
            if (nframes>10)
               nframes=10;
         } else if (strcmp(long_options[option_index].name,"comp")==0)
         {
            complexity = atoi (optarg);
#ifdef USE_SPEEXDSP
         } else if (strcmp(long_options[option_index].name,"denoise")==0)
         {
            denoise_enabled=1;
         } else if (strcmp(long_options[option_index].name,"agc")==0)
         {
            agc_enabled=1;
#endif
         } else if (strcmp(long_options[option_index].name,"no-highpass")==0)
         {
            highpass_enabled=0;
         } else if (strcmp(long_options[option_index].name,"skeleton")==0)
         {
            with_skeleton=1;
         } else if (strcmp(long_options[option_index].name,"help")==0)
         {
            usage();
            exit(0);
         } else if (strcmp(long_options[option_index].name,"quiet")==0)
         {
            quiet = 1;
         } else if (strcmp(long_options[option_index].name,"version")==0)
         {
            version();
            exit(0);
         } else if (strcmp(long_options[option_index].name,"version-short")==0)
         {
            version_short();
            exit(0);
         } else if (strcmp(long_options[option_index].name,"print-rate")==0)
         {
            output_rate=1;
         } else if (strcmp(long_options[option_index].name,"le")==0)
         {
            lsb=1;
         } else if (strcmp(long_options[option_index].name,"be")==0)
         {
            lsb=0;
         } else if (strcmp(long_options[option_index].name,"8bit")==0)
         {
            fmt=8;
         } else if (strcmp(long_options[option_index].name,"16bit")==0)
         {
            fmt=16;
         } else if (strcmp(long_options[option_index].name,"stereo")==0)
         {
            chan=2;
         } else if (strcmp(long_options[option_index].name,"rate")==0)
         {
            rate=atoi (optarg);
         } else if (strcmp(long_options[option_index].name,"comment")==0)
         {
            if (!strchr(optarg, '='))
            {
               fprintf (stderr, "Invalid comment: %s\n", optarg);
               fprintf (stderr, "Comments must be of the form name=value\n");
               exit(1);
            }
         //   comment_add(&comments, &comments_length, NULL, optarg);
         } else if (strcmp(long_options[option_index].name,"author")==0)
         {
         //   comment_add(&comments, &comments_length, "author=", optarg);
         } else if (strcmp(long_options[option_index].name,"title")==0)
         {
         //   comment_add(&comments, &comments_length, "title=", optarg);
         }

         break;
      case 'n':
         modeID = SPEEX_MODEID_NB;
         break;
      case 'h':
         usage();
         exit(0);
         break;
      case 'v':
         version();
         exit(0);
         break;
      case 'V':
         print_bitrate=1;
         break;
      case 'w':
         modeID = SPEEX_MODEID_WB;
         break;
      case 'u':
         modeID = SPEEX_MODEID_UWB;
         break;
      case '?':
         usage();
         exit(1);
         break;
      }
   }
   if (argc-optind!=2)
   {
      usage();
      exit(1);
   }
   inFile=argv[optind];
   outFile=argv[optind+1];


  start = clock();


   {
      fin = fopen(inFile, "rb");
      if (!fin)
      {
         perror(inFile);
         exit(1);
      }
      close_in=1;
   }

   {
      if (fread(first_bytes, 1, 12, fin) != 12)
      {
         perror("short file");
         exit(1);
      }

   }

   
   {
      if (modeID == SPEEX_MODEID_NB)
         rate=8000;
      else if (modeID == SPEEX_MODEID_WB)
         rate=16000;
      else if (modeID == SPEEX_MODEID_UWB)
         rate=32000;
   }

   if (!quiet)
      if (rate!=8000 && rate!=16000 && rate!=32000)
         fprintf (stderr, "Warning: Speex is only optimized for 8, 16 and 32 kHz. It will still work at %d Hz but your mileage may vary\n", rate);

   if (!mode)
      mode = speex_lib_get_mode (modeID);

   speex_init_header(&header, rate, 1, mode);
   header.frames_per_packet=nframes;
   header.vbr=vbr_enabled;
   header.nb_channels = chan;

   {
      char *st_string="mono";
      if (chan==2)
         st_string="stereo";
      if (!quiet)
         fprintf (stderr, "Encoding %d Hz audio using %s mode (%s)\n",
               header.rate, mode->modeName, st_string);
   }
   /*fprintf (stderr, "Encoding %d Hz audio at %d bps using %s mode\n",
     header.rate, mode->bitrate, mode->modeName);*/

   start = clock();
   /*Initialize Speex encoder*/
   st = speex_encoder_init(mode);


   {
      fout = fopen(outFile, "wb");
      if (!fout)
      {
         perror(outFile);
         exit(1);
      }
      close_out=1;
   }



   speex_encoder_ctl(st, SPEEX_GET_FRAME_SIZE, &frame_size);
   speex_encoder_ctl(st, SPEEX_SET_COMPLEXITY, &complexity);
   speex_encoder_ctl(st, SPEEX_SET_SAMPLING_RATE, &rate);

   if (quality >= 0)
   {
      if (vbr_enabled)
      {
         if (vbr_max>0)
            speex_encoder_ctl(st, SPEEX_SET_VBR_MAX_BITRATE, &vbr_max);
         speex_encoder_ctl(st, SPEEX_SET_VBR_QUALITY, &vbr_quality);
      }
      else
         speex_encoder_ctl(st, SPEEX_SET_QUALITY, &quality);
   }
   if (bitrate)
   {
      if (quality >= 0 && vbr_enabled)
         fprintf (stderr, "Warning: --bitrate option is overriding --quality\n");
      speex_encoder_ctl(st, SPEEX_SET_BITRATE, &bitrate);
   }
   if (vbr_enabled)
   {
      tmp=1;
      speex_encoder_ctl(st, SPEEX_SET_VBR, &tmp);
   } else if (vad_enabled)
   {
      tmp=1;
      speex_encoder_ctl(st, SPEEX_SET_VAD, &tmp);
   }
   if (dtx_enabled)
      speex_encoder_ctl(st, SPEEX_SET_DTX, &tmp);
   if (dtx_enabled && !(vbr_enabled || abr_enabled || vad_enabled))
   {
      fprintf (stderr, "Warning: --dtx is useless without --vad, --vbr or --abr\n");
   } else if ((vbr_enabled || abr_enabled) && (vad_enabled))
   {
      fprintf (stderr, "Warning: --vad is already implied by --vbr or --abr\n");
   }
 

   if (abr_enabled)
   {
      speex_encoder_ctl(st, SPEEX_SET_ABR, &abr_enabled);
   }

   speex_encoder_ctl(st, SPEEX_SET_HIGHPASS, &highpass_enabled);

   speex_encoder_ctl(st, SPEEX_GET_LOOKAHEAD, &lookahead);




   speex_bits_init(&bits);
   printf("frame_size: %d,fmt: %d,chan: %d,lsb: %d,lookahead: %d\n", frame_size,fmt,chan,lsb, lookahead);
  
   
   
   nb_samples = read_samples(fin,frame_size,fmt,chan,lsb,input, first_bytes, NULL);
   
   if (nb_samples==0)
      eos=1;
   total_samples += nb_samples;
   nb_encoded = -lookahead;
  
   /*Main encoding loop (one frame per iteration)*/
   while (!eos || total_samples>nb_encoded)
   {
      id++;
      /*Encode current frame*/
   

      speex_encode_int(st, input, &bits);

      nb_encoded += frame_size;
      if (print_bitrate) {
         int tmp;
         char ch=13;
         speex_encoder_ctl(st, SPEEX_GET_BITRATE, &tmp);
         fputc (ch, stderr);
         cumul_bits += tmp;
         enc_frames += 1;
         if (!quiet)
         {
            if (vad_enabled || vbr_enabled || abr_enabled)
               fprintf (stderr, "Bitrate is use: %d bps  (average %d bps)   ", tmp, (int)(cumul_bits/enc_frames));
            else
               fprintf (stderr, "Bitrate is use: %d bps     ", tmp);
            if (output_rate)
               printf ("%d\n", tmp);
         }

      }

      {
         nb_samples = read_samples(fin,frame_size,fmt,chan,lsb,input, NULL, NULL);
      }
      if (nb_samples==0)
      {
         eos=1;
      }

      total_samples += nb_samples;

      if ((id+1)%nframes!=0)
         continue;
      speex_bits_insert_terminator(&bits);
      nbBytes = speex_bits_write(&bits, cbits, MAX_FRAME_BYTES);
      fwrite(cbits, 1, nbBytes, fout);
      speex_bits_reset(&bits);
      
   }
   

   speex_encoder_destroy(st);
   speex_bits_destroy(&bits);

   if (close_in)
      fclose(fin);
   if (close_out)
      fclose(fout);


   end = clock();
   cpu_time_use = ( (double) end-start) / CLOCKS_PER_SEC;
   printf("took %f seconds to execute \n", cpu_time_use);

   return 0;
}

