
function entergraphview(lastbranch) {
	view = {}
	var canvas = view.canvas = document.createElement('canvas')
	canvas.width = window.innerWidth
	canvas.height = window.innerHeight


	view.leave = function() {
		// Setting these makes a bug in chrome dissapear.
		// Otherwise there is a chance that the screen will not
		// refresh properly
		canvas.width = 0
		canvas.height = 0
		body.removeChild(canvas)
	}

	var g = new Graph(canvas, Math.floor(window.clientWidth * 3/5), '20px serif normal')

	var active = undefined
	var lastmouse = undefined

	window.onmousedown = function(e) {
		lastmouse = {
			x: e.clientX, y: e.clientY } }
			
	window.onmousemove = function(e) {
		if(lastmouse != undefined) {
			g.x -= lastmouse.x - e.clientX
			g.y -= lastmouse.y - e.clientY

			lastmouse = { x: e.clientX, y: e.clientY }
		}

		for(var i in g.vertices) {
			var v = g.vertices[i]
			if(Math.abs(g.x + v.x - e.clientX) < v.size && 
				Math.abs(g.y + v.y - e.clientY) < v.size) {
				body.classList.add('clickable')
				active = v
				return;
			}
		}

		active = undefined
		body.classList.remove('clickable')
	}

			 
	window.onmouseup = function() {
		lastmouse = undefined
		if(active == undefined) return

		window.onmouseup = undefined
		window.onmousemove = undefined
		window.onmousedown = undefined


		g.ending = {
			time: new Date().getTime(),
			vertex: active,
			callback: function() {

				lastbranch = active.branch
				pointer = undefined
	
				body.classList.remove('clickable')
				view.leave()
				entereditorview(lastbranch)

			}
		}
	}
	

	for(var i in branches) {
		var color = undefined;
		if(branches[i].parent == 0) color = '#1987D1'

		var s = branches[i].data.length
		if(s < 1) s = 1
		s = 7 + Math.log(s) * 2

		
		var v = g.createVertex(i, color, s, branches[i].render)

		if(i == lastbranch.id)
			g.active = v

		v.branch = branches[i]	
	}
	

	for(var i in branches) {
		if(branches[i].parent != 0)
			g.createEdge(branches[i].parent, i)	
	}


	body.appendChild(canvas)
	g.go()
}

