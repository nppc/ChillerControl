EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:KiCadCustomLibraries
LIBS:ChillerControl-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L WeMos_mini U2
U 1 1 5B681310
P 5750 2500
F 0 "U2" H 5750 3000 60  0000 C CNN
F 1 "WeMos_mini" H 5750 2000 60  0000 C CNN
F 2 "wemos_D1_mini_board" H 5750 1900 60  0000 C CNN
F 3 "" H 6300 1800 60  0000 C CNN
	1    5750 2500
	1    0    0    -1  
$EndComp
$Comp
L MP1584ENBB U1
U 1 1 5B6813AE
P 5450 4000
F 0 "U1" H 5450 4000 60  0000 C CNN
F 1 "MP1584ENBB" H 5750 4500 60  0000 C CNN
F 2 "KiCadCustomLibs:MP1584_buck_module" H 5450 4000 60  0001 C CNN
F 3 "" H 5450 4000 60  0001 C CNN
	1    5450 4000
	1    0    0    -1  
$EndComp
$Comp
L R R1
U 1 1 5B68150A
P 4250 2250
F 0 "R1" H 4350 2250 50  0000 C CNN
F 1 "4.7K" V 4250 2250 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 4180 2250 50  0001 C CNN
F 3 "" H 4250 2250 50  0001 C CNN
	1    4250 2250
	1    0    0    -1  
$EndComp
$Comp
L R R2
U 1 1 5B681683
P 4550 2250
F 0 "R2" H 4650 2250 50  0000 C CNN
F 1 "4.7K" V 4550 2250 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 4480 2250 50  0001 C CNN
F 3 "" H 4550 2250 50  0001 C CNN
	1    4550 2250
	1    0    0    -1  
$EndComp
$Comp
L +3.3V #PWR16
U 1 1 5B681801
P 6250 2150
F 0 "#PWR16" H 6250 2000 50  0001 C CNN
F 1 "+3.3V" H 6250 2290 50  0000 C CNN
F 2 "" H 6250 2150 50  0001 C CNN
F 3 "" H 6250 2150 50  0001 C CNN
	1    6250 2150
	1    0    0    -1  
$EndComp
$Comp
L +3.3V #PWR11
U 1 1 5B68188F
P 4400 2100
F 0 "#PWR11" H 4400 1950 50  0001 C CNN
F 1 "+3.3V" H 4400 2240 50  0000 C CNN
F 2 "" H 4400 2100 50  0001 C CNN
F 3 "" H 4400 2100 50  0001 C CNN
	1    4400 2100
	1    0    0    -1  
$EndComp
Wire Wire Line
	4250 2100 4550 2100
Connection ~ 4400 2100
$Comp
L +5V #PWR17
U 1 1 5B681AA8
P 6350 3650
F 0 "#PWR17" H 6350 3500 50  0001 C CNN
F 1 "+5V" H 6350 3790 50  0000 C CNN
F 2 "" H 6350 3650 50  0001 C CNN
F 3 "" H 6350 3650 50  0001 C CNN
	1    6350 3650
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR15
U 1 1 5B681AC6
P 5250 2150
F 0 "#PWR15" H 5250 2000 50  0001 C CNN
F 1 "+5V" H 5250 2290 50  0000 C CNN
F 2 "" H 5250 2150 50  0001 C CNN
F 3 "" H 5250 2150 50  0001 C CNN
	1    5250 2150
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR18
U 1 1 5B681BC3
P 6350 3850
F 0 "#PWR18" H 6350 3600 50  0001 C CNN
F 1 "GND" H 6350 3700 50  0000 C CNN
F 2 "" H 6350 3850 50  0001 C CNN
F 3 "" H 6350 3850 50  0001 C CNN
	1    6350 3850
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR14
U 1 1 5B681C2F
P 5150 3850
F 0 "#PWR14" H 5150 3600 50  0001 C CNN
F 1 "GND" H 5150 3700 50  0000 C CNN
F 2 "" H 5150 3850 50  0001 C CNN
F 3 "" H 5150 3850 50  0001 C CNN
	1    5150 3850
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR12
U 1 1 5B681CCF
P 4800 2250
F 0 "#PWR12" H 4800 2000 50  0001 C CNN
F 1 "GND" H 4800 2100 50  0000 C CNN
F 2 "" H 4800 2250 50  0001 C CNN
F 3 "" H 4800 2250 50  0001 C CNN
	1    4800 2250
	1    0    0    -1  
$EndComp
Wire Wire Line
	5250 2250 4800 2250
$Comp
L +15V #PWR13
U 1 1 5B681DD3
P 5150 3650
F 0 "#PWR13" H 5150 3500 50  0001 C CNN
F 1 "+15V" H 5150 3790 50  0000 C CNN
F 2 "" H 5150 3650 50  0001 C CNN
F 3 "" H 5150 3650 50  0001 C CNN
	1    5150 3650
	1    0    0    -1  
$EndComp
Wire Wire Line
	3650 2650 5250 2650
Text Label 4950 2650 0    60   ~ 0
SCL
Wire Wire Line
	4000 2550 5250 2550
Text Label 4950 2550 0    60   ~ 0
SDA
Wire Wire Line
	4550 2550 4550 2400
Wire Wire Line
	4250 2650 4250 2400
$Comp
L R R4
U 1 1 5B681FFF
P 6750 2300
F 0 "R4" H 6850 2300 50  0000 C CNN
F 1 "4.7K" V 6750 2300 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 6680 2300 50  0001 C CNN
F 3 "" H 6750 2300 50  0001 C CNN
	1    6750 2300
	1    0    0    -1  
$EndComp
$Comp
L +3.3V #PWR19
U 1 1 5B6820E5
P 6750 2150
F 0 "#PWR19" H 6750 2000 50  0001 C CNN
F 1 "+3.3V" H 6750 2290 50  0000 C CNN
F 2 "" H 6750 2150 50  0001 C CNN
F 3 "" H 6750 2150 50  0001 C CNN
	1    6750 2150
	1    0    0    -1  
$EndComp
Wire Wire Line
	6250 2450 7750 2450
Text Label 6250 2450 0    60   ~ 0
Temp
$Comp
L CONN_01X03_MALE J9
U 1 1 5B6822F4
P 8300 2250
F 0 "J9" H 8300 2525 50  0000 C CNN
F 1 "Temp1" H 8325 1975 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x03_Pitch2.54mm" H 8300 2450 50  0001 C CNN
F 3 "" H 8300 2450 50  0001 C CNN
	1    8300 2250
	-1   0    0    1   
$EndComp
$Comp
L CONN_01X03_MALE J10
U 1 1 5B682431
P 8300 3050
F 0 "J10" H 8300 3325 50  0000 C CNN
F 1 "Temp2" H 8325 2775 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x03_Pitch2.54mm" H 8300 3250 50  0001 C CNN
F 3 "" H 8300 3250 50  0001 C CNN
	1    8300 3050
	-1   0    0    1   
$EndComp
$Comp
L CONN_01X03_MALE J2
U 1 1 5B68246A
P 3350 2650
F 0 "J2" H 3350 2925 50  0000 C CNN
F 1 "I2C" H 3375 2375 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x03_Pitch2.54mm" H 3350 2850 50  0001 C CNN
F 3 "" H 3350 2850 50  0001 C CNN
	1    3350 2650
	1    0    0    1   
$EndComp
$Comp
L GND #PWR26
U 1 1 5B6826BF
P 8000 2450
F 0 "#PWR26" H 8000 2200 50  0001 C CNN
F 1 "GND" H 8000 2300 50  0000 C CNN
F 2 "" H 8000 2450 50  0001 C CNN
F 3 "" H 8000 2450 50  0001 C CNN
	1    8000 2450
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR28
U 1 1 5B68275A
P 8000 3250
F 0 "#PWR28" H 8000 3000 50  0001 C CNN
F 1 "GND" H 8000 3100 50  0000 C CNN
F 2 "" H 8000 3250 50  0001 C CNN
F 3 "" H 8000 3250 50  0001 C CNN
	1    8000 3250
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR25
U 1 1 5B68277D
P 8000 2050
F 0 "#PWR25" H 8000 1900 50  0001 C CNN
F 1 "+5V" H 8000 2190 50  0000 C CNN
F 2 "" H 8000 2050 50  0001 C CNN
F 3 "" H 8000 2050 50  0001 C CNN
	1    8000 2050
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR27
U 1 1 5B682800
P 8000 2850
F 0 "#PWR27" H 8000 2700 50  0001 C CNN
F 1 "+5V" H 8000 2990 50  0000 C CNN
F 2 "" H 8000 2850 50  0001 C CNN
F 3 "" H 8000 2850 50  0001 C CNN
	1    8000 2850
	1    0    0    -1  
$EndComp
Wire Wire Line
	7750 3050 8000 3050
Wire Wire Line
	7750 2250 7750 3050
Wire Wire Line
	7750 2250 8000 2250
Connection ~ 7750 2450
Connection ~ 6750 2450
$Comp
L GND #PWR6
U 1 1 5B682D2B
P 3650 2850
F 0 "#PWR6" H 3650 2600 50  0001 C CNN
F 1 "GND" H 3650 2700 50  0000 C CNN
F 2 "" H 3650 2850 50  0001 C CNN
F 3 "" H 3650 2850 50  0001 C CNN
	1    3650 2850
	1    0    0    -1  
$EndComp
Connection ~ 4250 2650
Wire Wire Line
	3650 2450 4000 2450
Wire Wire Line
	4000 2450 4000 2550
Connection ~ 4550 2550
$Comp
L CONN_01X02 J3
U 1 1 5B6842F8
P 3400 3500
F 0 "J3" H 3400 3650 50  0000 C CNN
F 1 "12V" V 3500 3500 50  0000 C CNN
F 2 "KiCadCustomLibs:Conn2pin_2.54mm" H 3400 3500 50  0001 C CNN
F 3 "" H 3400 3500 50  0001 C CNN
	1    3400 3500
	-1   0    0    1   
$EndComp
$Comp
L GND #PWR2
U 1 1 5B68462D
P 3600 3550
F 0 "#PWR2" H 3600 3300 50  0001 C CNN
F 1 "GND" H 3600 3400 50  0000 C CNN
F 2 "" H 3600 3550 50  0001 C CNN
F 3 "" H 3600 3550 50  0001 C CNN
	1    3600 3550
	1    0    0    -1  
$EndComp
$Comp
L +12V #PWR1
U 1 1 5B68464C
P 3600 3450
F 0 "#PWR1" H 3600 3300 50  0001 C CNN
F 1 "+12V" H 3600 3590 50  0000 C CNN
F 2 "" H 3600 3450 50  0001 C CNN
F 3 "" H 3600 3450 50  0001 C CNN
	1    3600 3450
	1    0    0    -1  
$EndComp
$Comp
L Q_NMOS_GSD Q1
U 1 1 5B6865FF
P 7650 4150
F 0 "Q1" H 7850 4200 50  0000 L CNN
F 1 "SI2308" H 7850 4100 50  0000 L CNN
F 2 "TO_SOT_Packages_SMD:SOT-23" H 7850 4250 50  0001 C CNN
F 3 "" H 7650 4150 50  0001 C CNN
	1    7650 4150
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR23
U 1 1 5B686802
P 7750 4450
F 0 "#PWR23" H 7750 4200 50  0001 C CNN
F 1 "GND" H 7750 4300 50  0000 C CNN
F 2 "" H 7750 4450 50  0001 C CNN
F 3 "" H 7750 4450 50  0001 C CNN
	1    7750 4450
	1    0    0    -1  
$EndComp
$Comp
L R R7
U 1 1 5B686A0B
P 7300 4300
F 0 "R7" H 7400 4300 50  0000 C CNN
F 1 "10K" V 7300 4300 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 7230 4300 50  0001 C CNN
F 3 "" H 7300 4300 50  0001 C CNN
	1    7300 4300
	1    0    0    -1  
$EndComp
Wire Wire Line
	7300 4450 7750 4450
Wire Wire Line
	7750 4450 7750 4350
Wire Wire Line
	7300 4150 7450 4150
$Comp
L CONN_01X02 J7
U 1 1 5B68709A
P 8250 3900
F 0 "J7" H 8250 4050 50  0000 C CNN
F 1 "VENT1" V 8350 3900 50  0000 C CNN
F 2 "KiCadCustomLibs:Conn2pin_2.54mm" H 8250 3900 50  0001 C CNN
F 3 "" H 8250 3900 50  0001 C CNN
	1    8250 3900
	1    0    0    -1  
$EndComp
Wire Wire Line
	7750 3950 8050 3950
$Comp
L +12V #PWR29
U 1 1 5B6875B9
P 8050 3850
F 0 "#PWR29" H 8050 3700 50  0001 C CNN
F 1 "+12V" H 8050 3990 50  0000 C CNN
F 2 "" H 8050 3850 50  0001 C CNN
F 3 "" H 8050 3850 50  0001 C CNN
	1    8050 3850
	1    0    0    -1  
$EndComp
Connection ~ 7300 4150
Text Label 6700 4150 0    60   ~ 0
VENT1
$Comp
L Q_NMOS_GSD Q2
U 1 1 5B6879B4
P 7650 5200
F 0 "Q2" H 7850 5250 50  0000 L CNN
F 1 "SI2308" H 7850 5150 50  0000 L CNN
F 2 "TO_SOT_Packages_SMD:SOT-23" H 7850 5300 50  0001 C CNN
F 3 "" H 7650 5200 50  0001 C CNN
	1    7650 5200
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR24
U 1 1 5B6879BA
P 7750 5500
F 0 "#PWR24" H 7750 5250 50  0001 C CNN
F 1 "GND" H 7750 5350 50  0000 C CNN
F 2 "" H 7750 5500 50  0001 C CNN
F 3 "" H 7750 5500 50  0001 C CNN
	1    7750 5500
	1    0    0    -1  
$EndComp
$Comp
L R R8
U 1 1 5B6879C0
P 7300 5350
F 0 "R8" H 7400 5350 50  0000 C CNN
F 1 "10K" V 7300 5350 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 7230 5350 50  0001 C CNN
F 3 "" H 7300 5350 50  0001 C CNN
	1    7300 5350
	1    0    0    -1  
$EndComp
Wire Wire Line
	7300 5500 7750 5500
Wire Wire Line
	7750 5500 7750 5400
Wire Wire Line
	7300 5200 7450 5200
$Comp
L CONN_01X02 J8
U 1 1 5B6879CA
P 8250 4950
F 0 "J8" H 8250 5100 50  0000 C CNN
F 1 "LOAD1" V 8350 4950 50  0000 C CNN
F 2 "KiCadCustomLibs:Conn2pin_2.54mm" H 8250 4950 50  0001 C CNN
F 3 "" H 8250 4950 50  0001 C CNN
	1    8250 4950
	1    0    0    -1  
$EndComp
Wire Wire Line
	7750 5000 8050 5000
$Comp
L +12V #PWR30
U 1 1 5B6879D1
P 8050 4900
F 0 "#PWR30" H 8050 4750 50  0001 C CNN
F 1 "+12V" H 8050 5040 50  0000 C CNN
F 2 "" H 8050 4900 50  0001 C CNN
F 3 "" H 8050 4900 50  0001 C CNN
	1    8050 4900
	1    0    0    -1  
$EndComp
Connection ~ 7300 5200
Text Label 6700 5200 0    60   ~ 0
LOAD1
$Comp
L CONN_01X02 J5
U 1 1 5B687E8B
P 4000 3500
F 0 "J5" H 4000 3650 50  0000 C CNN
F 1 "VENT2" V 4100 3500 50  0000 C CNN
F 2 "KiCadCustomLibs:Conn2pin_2.54mm" H 4000 3500 50  0001 C CNN
F 3 "" H 4000 3500 50  0001 C CNN
	1    4000 3500
	-1   0    0    1   
$EndComp
$Comp
L +12V #PWR7
U 1 1 5B687FF9
P 4200 3450
F 0 "#PWR7" H 4200 3300 50  0001 C CNN
F 1 "+12V" H 4200 3590 50  0000 C CNN
F 2 "" H 4200 3450 50  0001 C CNN
F 3 "" H 4200 3450 50  0001 C CNN
	1    4200 3450
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR8
U 1 1 5B688034
P 4200 3550
F 0 "#PWR8" H 4200 3300 50  0001 C CNN
F 1 "GND" H 4200 3400 50  0000 C CNN
F 2 "" H 4200 3550 50  0001 C CNN
F 3 "" H 4200 3550 50  0001 C CNN
	1    4200 3550
	1    0    0    -1  
$EndComp
Wire Wire Line
	6250 2350 6600 2350
Text Label 6600 2350 2    60   ~ 0
VENT1
Text Label 6600 2550 2    60   ~ 0
LOAD1
$Comp
L R R5
U 1 1 5B68B73C
P 7150 4150
F 0 "R5" V 7250 4150 50  0000 C CNN
F 1 "100" V 7150 4150 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 7080 4150 50  0001 C CNN
F 3 "" H 7150 4150 50  0001 C CNN
	1    7150 4150
	0    1    1    0   
$EndComp
Wire Wire Line
	7000 4150 6700 4150
$Comp
L R R6
U 1 1 5B68BAAB
P 7150 5200
F 0 "R6" V 7250 5200 50  0000 C CNN
F 1 "100" V 7150 5200 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 7080 5200 50  0001 C CNN
F 3 "" H 7150 5200 50  0001 C CNN
	1    7150 5200
	0    1    1    0   
$EndComp
Wire Wire Line
	7000 5200 6700 5200
$Comp
L CONN_01X03_MALE J1
U 1 1 5B68CC15
P 3350 1900
F 0 "J1" H 3350 2175 50  0000 C CNN
F 1 "I2C2" H 3375 1625 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x03_Pitch2.54mm" H 3350 2100 50  0001 C CNN
F 3 "" H 3350 2100 50  0001 C CNN
	1    3350 1900
	1    0    0    1   
$EndComp
$Comp
L GND #PWR5
U 1 1 5B68CD2A
P 3650 2100
F 0 "#PWR5" H 3650 1850 50  0001 C CNN
F 1 "GND" H 3650 1950 50  0000 C CNN
F 2 "" H 3650 2100 50  0001 C CNN
F 3 "" H 3650 2100 50  0001 C CNN
	1    3650 2100
	1    0    0    -1  
$EndComp
Wire Wire Line
	3650 1700 3900 1700
Wire Wire Line
	3650 1900 3900 1900
Text Label 3900 1900 2    60   ~ 0
SCL
Text Label 3900 1700 2    60   ~ 0
SDA
Wire Wire Line
	6250 2550 6600 2550
$Comp
L LED D1
U 1 1 5B68ED79
P 7050 2650
F 0 "D1" H 7050 2750 50  0000 C CNN
F 1 "LED" H 7050 2550 50  0000 C CNN
F 2 "LEDs:LED_0805" H 7050 2650 50  0001 C CNN
F 3 "" H 7050 2650 50  0001 C CNN
	1    7050 2650
	-1   0    0    1   
$EndComp
$Comp
L R R3
U 1 1 5B68F00A
P 6600 2650
F 0 "R3" V 6700 2650 50  0000 C CNN
F 1 "680" V 6600 2650 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 6530 2650 50  0001 C CNN
F 3 "" H 6600 2650 50  0001 C CNN
	1    6600 2650
	0    1    1    0   
$EndComp
Wire Wire Line
	6250 2650 6450 2650
Wire Wire Line
	6750 2650 6900 2650
$Comp
L GND #PWR22
U 1 1 5B68F3A1
P 7200 2650
F 0 "#PWR22" H 7200 2400 50  0001 C CNN
F 1 "GND" H 7200 2500 50  0000 C CNN
F 2 "" H 7200 2650 50  0001 C CNN
F 3 "" H 7200 2650 50  0001 C CNN
	1    7200 2650
	1    0    0    -1  
$EndComp
$Comp
L CONN_01X02 J4
U 1 1 5B68FBBE
P 3400 4100
F 0 "J4" H 3400 4250 50  0000 C CNN
F 1 "DCIN" V 3500 4100 50  0000 C CNN
F 2 "KiCadCustomLibs:Conn_Power" H 3400 4100 50  0001 C CNN
F 3 "" H 3400 4100 50  0001 C CNN
	1    3400 4100
	-1   0    0    1   
$EndComp
$Comp
L GND #PWR4
U 1 1 5B68FC69
P 3600 4150
F 0 "#PWR4" H 3600 3900 50  0001 C CNN
F 1 "GND" H 3600 4000 50  0000 C CNN
F 2 "" H 3600 4150 50  0001 C CNN
F 3 "" H 3600 4150 50  0001 C CNN
	1    3600 4150
	1    0    0    -1  
$EndComp
$Comp
L +15V #PWR3
U 1 1 5B68FCB6
P 3600 4050
F 0 "#PWR3" H 3600 3900 50  0001 C CNN
F 1 "+15V" H 3600 4190 50  0000 C CNN
F 2 "" H 3600 4050 50  0001 C CNN
F 3 "" H 3600 4050 50  0001 C CNN
	1    3600 4050
	1    0    0    -1  
$EndComp
$Comp
L CONN_01X02 J6
U 1 1 5B6912A4
P 4000 4100
F 0 "J6" H 4000 4250 50  0000 C CNN
F 1 "DCIN" V 4100 4100 50  0000 C CNN
F 2 "KiCadCustomLibs:Conn_Power" H 4000 4100 50  0001 C CNN
F 3 "" H 4000 4100 50  0001 C CNN
	1    4000 4100
	-1   0    0    1   
$EndComp
$Comp
L GND #PWR10
U 1 1 5B6912AA
P 4200 4150
F 0 "#PWR10" H 4200 3900 50  0001 C CNN
F 1 "GND" H 4200 4000 50  0000 C CNN
F 2 "" H 4200 4150 50  0001 C CNN
F 3 "" H 4200 4150 50  0001 C CNN
	1    4200 4150
	1    0    0    -1  
$EndComp
$Comp
L +15V #PWR9
U 1 1 5B6912B0
P 4200 4050
F 0 "#PWR9" H 4200 3900 50  0001 C CNN
F 1 "+15V" H 4200 4190 50  0000 C CNN
F 2 "" H 4200 4050 50  0001 C CNN
F 3 "" H 4200 4050 50  0001 C CNN
	1    4200 4050
	1    0    0    -1  
$EndComp
$Comp
L LED D3
U 1 1 5B692A50
P 6900 5650
F 0 "D3" H 6900 5750 50  0000 C CNN
F 1 "LED" H 6900 5550 50  0000 C CNN
F 2 "LEDs:LED_0805" H 6900 5650 50  0001 C CNN
F 3 "" H 6900 5650 50  0001 C CNN
	1    6900 5650
	0    -1   -1   0   
$EndComp
Connection ~ 6900 5200
$Comp
L GND #PWR21
U 1 1 5B6935D6
P 6900 5800
F 0 "#PWR21" H 6900 5550 50  0001 C CNN
F 1 "GND" H 6900 5650 50  0000 C CNN
F 2 "" H 6900 5800 50  0001 C CNN
F 3 "" H 6900 5800 50  0001 C CNN
	1    6900 5800
	1    0    0    -1  
$EndComp
$Comp
L R R10
U 1 1 5B693731
P 6900 5350
F 0 "R10" V 7000 5350 50  0000 C CNN
F 1 "680" V 6900 5350 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 6830 5350 50  0001 C CNN
F 3 "" H 6900 5350 50  0001 C CNN
	1    6900 5350
	-1   0    0    1   
$EndComp
$Comp
L LED D2
U 1 1 5B693ED5
P 6900 4600
F 0 "D2" H 6900 4700 50  0000 C CNN
F 1 "LED" H 6900 4500 50  0000 C CNN
F 2 "LEDs:LED_0805" H 6900 4600 50  0001 C CNN
F 3 "" H 6900 4600 50  0001 C CNN
	1    6900 4600
	0    -1   -1   0   
$EndComp
$Comp
L R R9
U 1 1 5B694039
P 6900 4300
F 0 "R9" V 7000 4300 50  0000 C CNN
F 1 "680" V 6900 4300 50  0000 C CNN
F 2 "Resistors_SMD:R_0603" V 6830 4300 50  0001 C CNN
F 3 "" H 6900 4300 50  0001 C CNN
	1    6900 4300
	-1   0    0    1   
$EndComp
$Comp
L GND #PWR20
U 1 1 5B6940B6
P 6900 4750
F 0 "#PWR20" H 6900 4500 50  0001 C CNN
F 1 "GND" H 6900 4600 50  0000 C CNN
F 2 "" H 6900 4750 50  0001 C CNN
F 3 "" H 6900 4750 50  0001 C CNN
	1    6900 4750
	1    0    0    -1  
$EndComp
Connection ~ 6900 4150
$EndSCHEMATC
