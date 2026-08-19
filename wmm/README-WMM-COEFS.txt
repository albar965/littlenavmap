World Magnetic Model WMM2025
========================================================
Date December 17, 2024

WMM.COF					WMM2025 Coefficients file
						(Replace old WMM.COF (WMM2020 
						 or WMM2015) file with this)

1. Installation Instructions
==========================

WMM2025 GUI
-----------

Go to installed directory, find WMM.COF and remove it. 
Replace it with the new WMM.COF. 


WMM_Linux and WMM_Windows C software
------------------------------------

For the version <= WMM2020, replace the WMM.COF file in the "bin" directory with the
new provided WMM.COF file 


Your own software
-----------------
Depending on your installation, find the coefficient 
file, WMM.COF (unless renamed). Replace it with the new
WMM.COF file (renaming it appropriately if necessary).

If the coefficients are embedded in your software, you
may need to embed the new coefficients.

2. Installation Verification
============================

To confirm you are using the correct WMM.COF file open 
it and verify that the header is:

    2025.0            WMM-2025      11/13/2024

To assist in confirming that the installation of the new 
coefficient file is correct we provide a set of test 
values in this package.  Here are a few as an example:

Date     HAE   Lat   Long   Decl   Incl       H         X         Y        Z         F        Ddot   Idot   Hdot   Xdot   Ydot   Zdot   Fdot
2020.0    66    14    143    0.12  13.08   34916.9   34916.8      70.2    8114.9   35847.5   -0.1   -0.1   29.6   29.6  -41.4  -46.4   18.3
2020.0    18     0     21    1.05 -26.46   29316.1   29311.2     536.0  -14589.0   32745.6    0.1    0.1    0.0   -0.9   51.2   54.5  -24.3
2020.5     6   -36   -137   20.16 -52.21   25511.4   23948.6    8791.9  -32897.6   41630.3    0.0    0.0  -21.6  -21.6   -3.8   65.4  -64.9
2020.5    63    26     81    0.43  40.84   34738.7   34737.7     259.2   30023.4   45914.9    0.0    0.1    5.9    5.9    2.4  128.2   88.3

Where HAE is height above WGS-84 ellipsoid.




Model Software Support
======================

*  National Centers for Environmental Information (NCEI)
*  E/NE42 325 Broadway
*  Boulder, CO 80305 USA
*  Attn: Manoj Nair or Arnaud Chulliat
*  Phone:  (303) 497-4642 or -6522
*  Email:  geomag.models@noaa.gov
For more details about the World Magnetic Model visit 
http://www.ngdc.noaa.gov/geomag/WMM/DoDWMM.shtml


