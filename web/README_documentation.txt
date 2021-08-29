directory name                          content explanation

/                                       HTML files, irrespective of version of the web frontend (see documentation for /assets/YEAR-LETTER)
/assets                                 JS and CSS files of the original, legacy ui
/assets/YEAR-LETTER                     JS and CSS files belonging to the particular version of the web frontend identified by the creation YEAR of the version and a LETTER starting from "a" without quotes and increasing by 1 letter for every other version created that YEAR
/images                                 images native to the first party web frontend, not distinguished by for which version of the web frontend they are
/plugins                                third-party plugins to the web frontend
/plugins/example-plugin_documentation   documentation of how to create a plugin
/themes                                 theming for a particular version of the web frontend, divided into folders bearing the identifier of the version their content is for as their name (see documentation for /assets/YEAR-LETTER)



ATTENTION: for compatibility reasons, all prior versions of the web frontend should remain accessible and looking and functioning as they used to. For the reason of being able to easily identify files which belong to one such version, the web frontend version was invented.

NOTE: the version of the web frontend is NOT the version of the map HTML as passed to a plugin. The version of the map HTML as passed to a plugin is a number starting from 1 and increased by 1 for every compatibility-breaking change to the map.html file when delivered in a release. That map HTML version is located at the bottom of map.html. The current version 1 is currently intended to not change for the forseeable future.


DISCLAIMER: with a new web frontend version, plugin support and interfacing can change completely rendering existing plugins and their usage of the map HTML version obsolete BUT the idea of the author of these lines is that the plugin interface remains as is BUT this is solely to the discretion of any future author, as is him abiding by this folder structure using a and the web frontend version identifier as given above.



NOTE: the "toolbar" classes in index.html of web frontend version 2021-a is not the toolbar as meant with the toolbar API for plugins, rather these constitute "the menu". The toolbar API for plugins is like the toolbar inside map.html of web frontend version 2021-a.




Entry file of web frontend version 2021-a :  index.html