function history(steps) {
	this.steps = steps
	this.position = 0
	this.render = ''
}

history.prototype = {
	forward: function() {
		if(this.position > this.steps.length - 1) return

		var change = this.steps[this.position]
		this.position++

		this.render = this.render.substr(0, change.pos) +
			change.now + this.render.substr(change.pos + change.last.length,
				this.render.length)
	},
	
	back: function() {
		if(this.position <= 0) return;

		var change = this.steps[this.position - 1]
		this.position--;

		this.render = this.render.substr(0, change.pos) +
			change.last + this.render.substr(change.pos + change.now.length,
				this.render.length)
	},

	go: function(go) {
		if(this.position == go) return;
		if(go <= 0) go = 0
		if(go > this.steps.length) go = this.steps.length


		while(this.position != go)
			if(this.position > go) this.back()
			else this.forward()
		
	}
}


