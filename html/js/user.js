function fetchupdates()
{
	request('/api/0/branches', undefined, 
		function(req)
		{
			var t = req.responseText.split('\n')
			

			for(var i = 0; i < t.length - 1; i += 2)
			{
				var r = t[i].split(' ')
				var b = new branch({
					id: parseInt(r[0], 10),
					parent: parseInt(r[1], 10),
					pos: parseInt(r[2], 10),
					document: parseInt(r[3], 10) 
				})
			}
	
			view.leave()
			enteruserview()
		}, 
		function() { alert('error loading documents') })
}


