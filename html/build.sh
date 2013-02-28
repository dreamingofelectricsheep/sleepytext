
compiled="<html><head><title>Sleepy Text</title><meta name=\"viewport\" content=\"user-scalable=no\" />"

js=`ls html/js`
for file in $js
do
	compiled=$compiled'<script type="text/javascript">'`cat html/js/$file`'</script>'
done

compiled=$compiled'<style type="text/css">'`cat html/style.css`'</style>'

compiled=$compiled'<body></body></html>'

echo "$compiled" > html/main.html
