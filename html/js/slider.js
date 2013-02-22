function slider(parent, onchange) {
	this.parent = parent
	this.bar = document.createElement('div')
	this.bar.setAttribute('class', 'slider-bar')
	this.knob = document.createElement('div')

	var that = this

	this.knob.onmousedown = this.bar.onmousedown = function(e) {
		body.classList.add('unselectable')
		body.classList.add('clickable')

		var f = function(e) {
			var offset = e.clientX - that.bar.offsetLeft 

			var p = offset / that.bar.offsetWidth			
			that.to(p)
		}

		f(e)

		window.onmousemove = f 
		window.onmouseup = function() {
			body.classList.remove('unselectable')
			body.classList.remove('clickable')
	
			window.onmousemove = undefined
			window.onmouseup = undefined
		}
	}
			
	this.bar.appendChild(this.knob)
	parent.appendChild(this.bar)

	this.to(1)

	this.onchange = onchange
}

slider.prototype = {
	to: function(p, logic) {
		if(p < 0) p = 0
		if(p > 1) p = 1

		this.knob.style.left = 100 * p + '%'

		this.position = p

		if(this.onchange == undefined || logic == true) return
		this.onchange(this.position)
	},
	despawn: function() {
		this.parent.removeChild(this.bar)
	}
}
