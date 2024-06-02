Features:

	Web API		->	webapi.yaml
	Plugins		->	plugins/example-plugin_documentation/index.html
	Themes		->	themes/2021-a/look-userdefined-theme-template.css
	Legacy mode	->	legacy.html


Web frontend version:

	a naming scheme used in structuring files which represent the
	Web frontend UI / UX seeable and interactable with in the web browser 
	into folders

	YEAR-letter :
		creation YEAR of the version and a letter starting from "a"
		without quotes and increasing by 1 letter for every further
		version created that YEAR

	ATTENTION:
		for compatibility reasons, all prior versions of the current
		web frontend should remain accessible and looking and
		functioning as they used to.
 

Folders:

	/			HTML files, irrespective of version of the web frontend
	/assets/		JS and CSS files of the original, legacy ui
	/assets/*/		JS and CSS files belonging to the particular version of the web frontend
	/images/		images native to the first party web frontend, irrespective of its version
	/plugins/		plugins
	/themes/*/		themes belonging to the particular version of the web frontend


Entry file of web frontend version 2021-a :

	/index.html


DISCLAIMER:
	with a new web frontend version, plugin support and interfacing can change completely rendering existing
	plugins and their usage of the map HTML version obsolete BUT the idea of the author of these lines is
	that the plugin interface remains as is BUT this is solely to the discretion of any future author,
	as is him abiding by this folder structure using the web frontend version identifier as given above.