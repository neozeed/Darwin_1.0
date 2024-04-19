/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


const char * gIOKernelConfigTables =
"(
    {
	'IOClass'		= IOHIDSystem;
	'IOProviderClass'	= IOResources;
	'IOResourceMatch'	= IOKit;
	'IOMatchCategory'	= IOHID;
    },
    {
	'IOClass'		= IOBSDConsole;
	'IOProviderClass'	= IOResources;
	'IOResourceMatch'	= IOBSD;
	'IOMatchCategory'	= IOBSDConsole;
    },
    {
	'IOClass'		= IODisplayWrangler;
	'IOProviderClass'	= IOResources;
	'IOResourceMatch'	= IOKit;
	'IOMatchCategory'	= IOGraphics;
    },
    {
        'IOClass'		= IOAudioManager;
        'IOProviderClass'	= IOResources;
        'IOResourceMatch'	= IOKit;
        'IOMatchCategory'	= IOAudio;
    },
    {
	'IOClass'		= IOApplePartitionScheme;
	'IOProviderClass'	= IOMedia;
	'IOProbeScore'		= 1200:32;
	'IOMatchCategory'	= IOStorage;
	'Content Mask'		= 'Apple_partition_scheme';
    },
    {
	'IOClass'		= IONeXTPartitionScheme;
	'IOProviderClass'	= IOMedia;
	'IOProbeScore'		= 1000:32;
	'IOMatchCategory'	= IOStorage;
	'Content Mask'		= 'NeXT_partition_scheme';
    },
    {
	'IOClass'		= IOFDiskPartitionScheme;
	'IOProviderClass'	= IOMedia;
	'IOProbeScore'		= 1100:32;
	'IOMatchCategory'	= IOStorage;
	'Content Mask'		= 'FDisk_partition_scheme';
    },
    {
	'IOClass'		= IOMediaBSDClient;
	'IOProviderClass'	= IOResources;
	'IOMatchCategory'	= IOMediaBSDClient;
	'IOResourceMatch'	= IOBSD;
    },
    {
	'IOClass'		= AppleDDCDisplay;
	'IOProviderClass'	= IODisplayConnect;
	'IOProbeScore'		= 2000:32;
	appleDDC		=   <00000082 00ff2140 0000008c 00043147 "
                                    "00000096 00053140 00000098 0003314c "
                                    "0000009a 0002314f 0000009c 00ff3159 "
                                    "000000aa 000d494f 000000b4 0001fffc "
                                    "000000b6 00004540 000000b8 000f454c "
                                    "000000ba 000e454f 000000bc 00ff4559 "
                                    "000000be 000b6140 000000c8 000a614a "
                                    "000000cc 0009614f 000000d0 00ff6159 "
                                    "000000d2 00ff614f 000000dc 0017ffc4 "
                                    "000000fa 00ff814f 00000104 00ff8180 "
                                    "00000106 0008818f 0000010c 00ff8199 "
                                    "00000118 00ffa940 0000011a 00ffa945 "
                                    "0000011c 00ffa94a 0000011e 00ffa94f "
                                    "00000120 00ffa954>;
        overrides		= ( { ID = 0x06105203:32;
					additions = <0000010c>; },
				    { ID = 0x06101092:32;
					additions = <00000121>; },
				    { ID = 0x0610029d:32;
					additions = <0000009e>; } );
    },
    {
	'IOClass'		= AppleG3SeriesDisplay;
	'IOProviderClass'	= IODisplayConnect;
	'IOProbeScore'		= 1500:32;
    },
    {
	'IOClass'		= AppleSenseDisplay;
	'IOProviderClass'	= IODisplayConnect;
	'IOProbeScore'		= 1000:32;
    },
    {
	'IOClass'		= AppleNoSenseDisplay;
	'IOProviderClass'	= IODisplayConnect;
	'IOProbeScore'		= 500:32;
    },
    {
	'IOClass'		= IOHDDrive;
	'IOProviderClass'	= IOHDDriveNub;
    },
    {
	'IOClass'		= IOSCSIHDDrive;
	'IOProviderClass'	= IOSCSIDevice;
    },
    {
	'IOClass'		= IOCDDrive;
	'IOProviderClass'	= IOCDDriveNub;
    },
    {
        'IOClass Names'         = IOCDAudioNubClient;
        'IOImports'             = IOCDAudioNub;
    },
    {
	'IOClass'		= IOSCSICDDrive;
	'IOProviderClass'	= IOSCSIDevice;
    },
    {
       'IOClass'           = IOATAHDDrive;
       'IOProviderClass'   = IOATADevice;
    },
    {
       'IOClass'           = IOATAPIHDDrive;
       'IOProviderClass'   = IOATADevice;
    },
    {
       'IOClass'           = IOATAPICDDrive;
       'IOProviderClass'   = IOATADevice;
    },
    {
	'IOClass'		= IONetworkStack;
	'IOProviderClass'	= IONetworkInterface;
	'IOResourceMatch'	= IOBSD;
    }
"
#ifdef PPC
"   ,
    {
	'IOClass'		= AppleCPU;
	'IOProviderClass'	= IOPlatformDevice;
        'IONameMatch'		= 'cpu';
	'IOProbeScore'		= 100:32;
    },
    {
        'IOClass'		= PowerSurgePE;
        'IOProviderClass'	= IOPlatformExpertDevice;
        'IONameMatch'		= ('AAPL,7300', 'AAPL,7500', 'AAPL,8500', 'AAPL,9500');
        'IOProbeScore'		= 20000:32;
    },
    {
        'IOClass'		= PowerStarPE;
        'IOProviderClass'	= IOPlatformExpertDevice;
        'IONameMatch'		= ('AAPL,3400/2400', 'AAPL,3500');
        'IOProbeScore'		= 10000:32;
    },
    {
        'IOClass'		= GossamerPE;
        'IOProviderClass'	= IOPlatformExpertDevice;
	'IONameMatch'		= ('AAPL,Gossamer', 'AAPL,PowerMac G3', 'AAPL,PowerBook1998', 'iMac,1', 'PowerMac1,1', 'PowerMac1,2', 'PowerBook1,1');
	'IOProbeScore'		= 10000:32;
    },
    {
        'IOClass'         	= PowerExpressPE;
        'IOProviderClass'	= IOPlatformExpertDevice;
	'IONameMatch'		= 'AAPL,9700';
	'IOProbeScore'		= 10000:32;
	'senses'		= <00000000 00000000 00000000 00000000 "
                                  "00000000 00000000 00000000 00000000 "
                                  "00000000 00000000 00000000 00000000 "
                                  "00000000 00000000 00000000 00000000 "
                                  "00000000 00000000 00000000 00000000 "
                                  "00000000 00000000 00000000 00000000 "
                                  "00000000 00000000 00000001 00000001 "
                                  "00000001 00000001 00000001 00000001 "
                                  "00000001 00000001 00000001 00000001 "
                                  "00000001 00000001>;
    },
    {
        'IOClass'         	= Core99PE;
        'IOProviderClass'	= IOPlatformExpertDevice;
	'IONameMatch'		= ('PowerMac2,1', 'PowerMac3,1', 'PowerBook2,1', 'PowerBook3,1');
	'IOProbeScore'		= 10000:32;
    },
"
"
    {
	'IOClass'		= AppleGracklePCI;
	'IOProviderClass'	= IOPlatformDevice;
	'IONameMatch'		= ('grackle', 'MOT,PPC106');
    },
    {
	'IOClass'		= AppleMacRiscPCI;
	'IOProviderClass'	= IOPlatformDevice;
	'IONameMatch'		= ('bandit', 'uni-north');
    },
    {
	'IOClass'		= AppleMacRiscAGP;
	'IOProviderClass'	= IOPlatformDevice;
	'IONameMatch'		= 'uni-north';
	'IOProbeScore'		= 1000:32;
    },
    {
	'IOClass'		= AppleMacRiscVCI;
	'IOProviderClass'	= IOPlatformDevice;
	'IONameMatch'		= chaos;
    },
    {
	'IOClass'		= IOPCI2PCIBridge;
	'IOProviderClass'	= IOPCIDevice;
	'IONameMatch'		= 'pci-bridge';
    },
    {
	'IOClass'		= IOPCI2PCIBridge;
	'IOProviderClass'	= IOPCIDevice;
        'IOPCIMatch'           = '0x00261011';
    },
    {
        'IOClass'		= GrandCentral;
        'IOProviderClass'	= IOPCIDevice;
        'IONameMatch'		= gc;
	'IOProbeScore'		= 2000:32;
    },
    {
        'IOClass'		= OHare;
        'IOProviderClass'	= IOPCIDevice;
        'IONameMatch'		= ('ohare', 'pci106b,7');
    },
    {
        'IOClass'		= Heathrow;
        'IOProviderClass'	= IOPCIDevice;
        'IONameMatch'		= ('heathrow', 'gatwick');
	'IOProbeScore'		= 4000:32;
	'vectors-escc-ch-a'	= (<0000000f>,<00000004>,<00000005>);
	'vectors-floppy'	= (<00000013>,<00000001>);
	'vectors-ata4'		= (<0000000e>,<00000003>);
    },
    {
        'IOClass'		= KeyLargo;
        'IOProviderClass'	= IOPCIDevice;
        'IONameMatch'		= 'Keylargo';
    },
    {
        'IOClass'		= AppleMPICInterruptController;
        'IOProviderClass'	= IOPlatformDevice;
        'IONameMatch'		= 'open-pic';
    },
    {
        'IOClass'		= AppleMPICInterruptController;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= 'open-pic';
    },
    {
        'IOClass'		= AppleNMI;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= 'programmer-switch';
    },
    {
        'IOClass'         	= AppleVIA;
        'IOProviderClass'       = AppleMacIODevice;
        'IONameMatch'		= ('via-cuda', 'via-pmu');
	'vectors'		= (<00000000>,<00000001>,<00000002>,<00000003>,
                                   <00000004>,<00000005>,<00000006>);
    },
    {
        'IOClass'		= AppleCuda;
        'IOProviderClass'	= AppleVIADevice;
        'IONameMatch'		= cuda;
    },
    {
        'IOClass'		= ApplePMU;
        'IOProviderClass'	= AppleVIADevice;
        'IONameMatch'		= pmu;
    },
    {
        'IOClass'		= Core99NVRAM;
        'IOProviderClass'	= IOPlatformDevice;
        'IONameMatch'		= 'nvram,flash';
    },
    {
        'IOClass'		= AppleNVRAM;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= nvram;
    },
    {
	'IOClass'		= IOADBBus;
	'IOProviderClass'	= IOADBController;
    },
  {
      'IOClass'			= AppleADBKeyboard;
      'IOProviderClass'		= IOADBDevice;
      'ADB Match'		= '2';
  },
  {
      'IOClass'			= AppleADBButtons;
      'IOProviderClass'		= IOADBDevice;
      'ADB Match'		= '7';
  },
    {
	'IOClass'		= AppleADBMouseType1;
	'IOProviderClass'	= IOADBDevice;
	'ADB Match'		= '3';
	'IOProbeScore'		= 5000:32;
    },
    {
	'IOClass'		= AppleADBMouseType2;
	'IOProviderClass'	= IOADBDevice;
	'ADB Match'		= '3';
	'IOProbeScore'		= 10000:32;
    },
    {
	'IOClass'		= AppleADBMouseType4;
	'IOProviderClass'	= IOADBDevice;
	'ADB Match'		= '3-01';
	'IOProbeScore'		= 20000:32;
    },
    {
	'IOClass'		= IONDRVFramebuffer;
	'IOProviderClass'	= IOPCIDevice;
	'IONameMatch'		= display;
	'IOProbeScore'		= 20000:32;
    },
    {
	'IOClass'		= IONDRVFramebuffer;
	'IOProviderClass'	= IOPlatformDevice;
	'IONameMatch'		= 'display';
	'IOProbeScore'		= 20000:32;
    },
    {
	'IOClass'		= IOBootFramebuffer;
	'IOProviderClass'	= IOPCIDevice;
	'IONameMatch'		= display;
    },
    {
	'IOClass'		= AppleADBDisplay;
	'IOProbeScore'		= 1000:32;
	'IOProviderClass'	= IOADBDevice;
	'ADB Match'		= '*-c0';
	modes850		=   <000000dc 0000008c 0000009a 0000009e "
                                    "000000aa 000000d2 000000d0 000000fa "
                                    "00000106 0000010c 00000118 0000011a "
                                    "0000011c 0000011e>;
	modes750		=   <000000dc 0000008c 000000aa 000000d2 "
                                    "000000fa 00000106 00000118>;
	modesStudio		=   <000000d2 0000008c 000000aa>;
	adb2Modes		= modes750;
	adb3Modes		= modes850;
	adb4Modes		= modes850;
	adb5Modes		= modes750;
	adb6Modes		= modesStudio;
    },
    {
        'IOClass'		= AppleOHCI;
        'IOProviderClass'	= IOPCIDevice;
        'IONameMatch'		= ('pci1095,670', 'pci1045,c861', 'pci106b,19', 'pci11c1,5801', 'pciclass,0c0310', 'usb');
    },
    {
        'IOClass'		= IOUSBHub;
        'IOProviderClass'	= IOUSBDevice;
	'class'			= 9:8;
        'IOProbeScore'		= 10000:32;
    },
    {
        'IOClass'		= AppleComposite;
        'IOProviderClass'	= IOUSBDevice;
        'class'			= 0:8;
        'IOProbeScore'		= 1000:32;
    },
    {
        'IOClass'		= AppleMouse;
        'IOProviderClass'	= IOUSBInterface;
        'class'			= 3:8;
        'protocol'		= 2:8;
        'IOProbeScore'		= 10000:32;
    },
    {   
        'IOClass'        	= AppleKeyboard;
        'IOProviderClass'	= IOUSBInterface;
        'class'			= 3:8;
        'protocol'		= 1:8;
        'IOProbeScore'		= 10000:32;
    },  
    {
	'IOClass'		= BMacEnet;
	'IOProviderClass'	= AppleMacIODevice;
	'IONameMatch'		= ('bmac', 'bmac+');
	'IOEnableDebugger'	= Yes;
    },
    {
	'IOClass'		= UniNEnet;
	'IOProviderClass'	= IOPCIDevice;
	'IONameMatch'		= ('gmac', 'SUNW,pci-gem');
	'IOEnableDebugger'  	= Yes;
    },
"
"
    {
        'IOClass'		= PPCAwacs;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= ('davbus', 'awacs');
    },
    {
        'IOClass'		= PPCBurgundy;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= ('davbus', 'perch');
    },
    {
        'IOClass'		= PPCDACA;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= 'i2s-a';
    },
    {
        'IOClass'		= CurioSCSIController;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= '53c94';
    },
    {
        'IOClass'		= meshSCSIController;
        'IOProviderClass'	= AppleMacIODevice;
        'IONameMatch'		= 'mesh';
    },
"
"
    {
        'IOClass'         	= Sym8xxSCSIController;
        'IOProviderClass'	= IOPCIDevice;
        'IONameMatch'		= ('apple53C8xx', 'Apple53C875Card', 'ATTO,ExpressPCIProLVD', 'ATTO,ExpressPCIProUL2D');
    },
    {
	'IOClass'		= MaceEnet;
	'IOProviderClass'	= AppleMacIODevice;
	'IONameMatch'		= mace;
	'IOEnableDebugger'	= Yes;
    },
    {
	'IOClass'		= AppleATAPPC;
	'IOProviderClass'	= AppleMacIODevice;
	'IONameMatch'		= ('ide', 'IDE', 'ata', 'ATA');
    },
    {
	'IOClass'		= AppleATAUltra646;
	'IOProviderClass'	= IOPCIDevice;
	'IONameMatch'		= 'pci-ata';
    },
"
"
    {
        'IOClass'           = Intel82557;
        'IOProviderClass'   = IOPCIDevice;
        'IOPCIMatch'        = '0x12298086';
        'IODefaultMedium'   = Auto;
        'IOEnableDebugger'  = No;
        'Flow Control'      = 1:32;
        'Verbose'           = 0:32;
    }
"
#endif /* PPC */
#ifdef i386
"   ,
    {
       'IOClass'           = AppleI386PlatformExpert;
       'IOProviderClass'   = IOPlatformExpertDevice;
       'top-level'         = "
    /* set of dicts to make into nubs */
    "[
       { IOName = cpu; },
       { IOName = intel-pic; },
       { IOName = intel-clock; }, 
       { IOName = ps2controller; },
       { IOName = pci; },
       { IOName = display; 'AAPL,boot-display' = Yes; }
    ];
    },
    {
       'IOClass'           = AppleI386CPU;
       'IOProviderClass'   = IOPlatformDevice;
       'IONameMatch'       = cpu;
       'IOProbeScore'      = 100:32;
    },
    {
       'IOClass'           = AppleIntelClassicPIC;
       'IOProviderClass'   = IOPlatformDevice;
       'IONameMatch'       = intel-pic;
    },
    {
       'IOClass'           = AppleIntelClock;
       'IOProviderClass'   = IOPlatformDevice;
       'IONameMatch'       = intel-clock;
    },
    {
       'IOClass'           = AppleI386PCI;
       'IOProviderClass'   = IOPlatformDevice;
       'IONameMatch'       = pci;
    },
    {
       'IOClass'           = ApplePS2Controller;
       'IOProviderClass'   = IOPlatformDevice;
       'IONameMatch'       = ps2controller;
    },
    {
       'IOClass'           = ApplePS2Keyboard;
       'IOProviderClass'   = ApplePS2KeyboardDevice;
    },
    {
       'IOClass'           = ApplePS2Mouse;
       'IOProviderClass'   = ApplePS2MouseDevice;
    },
    {
       'IOClass'           = IOBootFramebuffer;
       'IOProviderClass'   = IOPlatformDevice;
       'IONameMatch'       = display;
    },
    {
       'IOClass'           = AppleATAPIIX;
       'IOProviderClass'   = IOPCIDevice;
       'IOPCIMatch'        = '0x12308086 0x70108086 0x71118086';
       'IOMatchCategory'   = AppleATAPIIXChannel0;
    },
    {
       'IOClass'           = AppleATAPIIX;
       'IOProviderClass'   = IOPCIDevice;
       'IOPCIMatch'        = '0x12308086 0x70108086 0x71118086';
       'IOMatchCategory'   = AppleATAPIIXChannel1;
    },
    {
       'IOClass'           = Intel82557;
       'IOProviderClass'   = IOPCIDevice;
       'IOPCIMatch'        = '0x12298086';
       'IODefaultMedium'   = Auto;
       'IOEnableDebugger'  = Yes;
       'Flow Control'      = 1:32;
       'Verbose'           = 0:32;
    },
"
#endif /* i386 */
")";
