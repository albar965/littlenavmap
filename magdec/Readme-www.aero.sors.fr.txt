Updated Magnetic variation data for FS9 & FSX-P3D
-------------------------------------------------

Magnetic variation data calculated by FS9 date back from 1988 and have significantly changed (several degrees in some part of the world).
Those used by FSX are more recent (2009) but are now also outdated. The same is true for FSX-SE & P3D (all versions) whose data are identical to those of FSX.
FS9, FSX and P3D calculate magnetic variation using a special latitude/longitude table that is contained in the MAGDEC.BGL file.
More details here: http://www.fsdeveloper.com/wiki/index.php?title=Magdec_BGL_File

The new provided MAGDEC.BGL table corrects all magnetic variations for FS2004 & FSX/P3D based on values calculated from the IGRF-12 models at epox 2018 (January 1st)
http://www.ngdc.noaa.gov/IAGA/vmod/igrf.html
and coded to the special normalized representation used in the MAGDEC.BGL file

Some additional details on how the simulator use the different "magnetic variation" data (magdec file, navaid and airport magnetic declinations) can be read here
http://aerosors.freeforums.net/thread/112/ils-reported-problems-airac-data

Updated MAGDEC.BGL files for both FS2004 and FSX-P3D on 1/1/2018 are provided.

Installation of the new MAGDEC.BGL files
----------------------------------------
1) Close FS9 or FSX/P3D, since you will not be allowed to replace the file while the simulator is running
2) Locate the MAGDEC.BGL file which is in the \SCENERY\BASE\SCENERY\ sub folder of your FS9/FSX-P3D install directory
3) KEEP A COPY of the old file somewhere or rename it as MAGDEC.BGL.BAK (DO NOT USE a bgl extension if the file is kept in its native directory)
4) In the provided package, select the updated file you want to use, either FS9 or FSX(P3D)
4) Copy this new MAGDEC.BGL file in the \SCENERY\BASE\SCENERY\ sub folder of your FS9/FSX-P3D install directory
That's all

Flight Simulator will rebuild its index at first launch and the new magnetic variations will be applied.

Disclaimer statement
--------------------
This package and its content are provided "as is" without warranty of any kind. I won't be liable for any damage that may be caused by it. It is released as freeware. As so, you are permitted to distribute it on any free media and on any mailbox or network that does not have a per-file download charge. If you like to include it to your own program, package or web site, please ask me.


Hervé Sors (c)2017-2018
http://www.aero.sors.fr