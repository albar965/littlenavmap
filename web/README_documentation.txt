Features:

	Web API		->	webapi.yaml
	Plugins		->	plugins/example-plugin_documentation/index.html
	Themes		->	themes/2021-a/look-userdefined-theme-template.css
	Legacy mode	->	html/legacy/index.html


Web frontend version:

	a naming scheme used in structuring files which represent the
	Web frontend UI / UX seeable and interactable with in the web browser 
	into folders

	YEAR-letter :
		creation YEAR of the version and a letter starting from "a"
		without quotes and increasing by 1 letter for every further
		version created that YEAR

        legacy :
                name for the original, legacy web frontend

	ATTENTION:
		for compatibility reasons, all prior versions of the current
		web frontend should remain accessible and looking and
		functioning as they used to.
 

Folders:

	/                               general-purpose files *1
	/html/                          html files *1
        /html/*/                        html files *2
	/images/                        image files *1
	/javascript_and_css/            JS and CSS files *1
	/javascript_and_css/*/          JS and CSS files *2
	/plugins/                       plugins *1
	/themes/*/                      themes *2

        *1 = irrespective of version of the web frontend
	*2 = belonging to a particular version of the web frontend


/index.html :

        loads the index file of the current web frontend version


Entry file of web frontend version 2021-a :

	/html/2021-a/index.html


DISCLAIMER:
	with a new web frontend version, plugin support and interfacing can change completely rendering existing
	plugins and their usage of the map HTML version obsolete BUT the idea of the author of these lines is
	that the plugin interface remains as is BUT this is solely to the discretion of any future author,
	as is him abiding by this folder structure using the web frontend version identifier as given above.