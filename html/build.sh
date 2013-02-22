
compiled="<html><head><title>Sleepy Text</title>"

js=`ls js`
for file in $js
do
	compiled=$compiled'<script type="text/javascript">'`cat js/$file`'</script>'
done

compiled=$compiled'<style type="text/css">'`cat style.css`'</style>'

compiled=$compiled'<body></body></html>'

echo "$compiled" > main.html
