This directory bundle ArcticViewer for Tomviz usage.

To update and build the given viewer, just run the following commands:

```
$ npm install
$ npm run build:release
```

Then the following directory content should be dropped along the exported data

```
${tomviz_src}/web/www/**
```

## Using file:// protocolin browsers:

Change local files security policy

### Safari

Enable the develop menu using the preferences panel, under Advanced -> "Show develop menu in menu bar"

Then from the safari "Develop" menu, select "Disable local file restrictions", it is also worth noting safari has some odd behaviour with caches, so it is advisable to use the "Disable caches" option in the same menu; if you are editing & debugging using safari.

### Chrome

Close all running chrome instances first. Then start Chrome executable with a command line flag:

chrome --allow-file-access-from-files
On Windows, the easiest is probably to create a special shortcut which has added flag (right-click on shortcut -> properties -> target).

### Firefox

Go to about:config
Find security.fileuri.strict_origin_policy parameter
Set it to false
