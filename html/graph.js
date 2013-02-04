function Graph(canvas, textwidth, font) {
	this.x = 0
	this.y = 0
	this.font = font
	this.canvas = canvas
	this.ctx = canvas.getContext('2d')
	this.vertices = {};
	this.edges = []
	this.forcex = {};
	this.forcey = {};
	this.stepsize = 0.0005;
	this.iteration = 0;
	this.task = null;
	this.textwidth = textwidth

	// tunables to adjust the layout
	this.repulsion = 200000; // repulsion constant, adjust for wider/narrower spacing
	this.spring_length = 50; // base resting length of springs
}

Graph.prototype.createVertex = function(id, color, size, text) { 
	if( color === undefined ) 
		color = "#64B80B";

	var vertex = {
		color: color,
		size: size,
		edges: []
	}

	vertex.x = this.canvas.width / 2 * (Math.random() + 1) * 0.5
	vertex.y = this.canvas.height / 2 * (Math.random() + 1) * 0.5

	this.vertices[id] = vertex
	return vertex
}

Graph.prototype.createEdge = function(a, b, color, size) {
	if(color == undefined)
		color = '#86D479'

	if(size == undefined)
		size = 5
	
	var line = {
		color: color,
		size: size,
		a: this.vertices[a],
		b: this.vertices[b]
	}
		
	this.vertices[a].edges[b] = { "dest" : b, "line": line };
	this.vertices[b].edges[a] = { "dest" : a, "line": line };

	this.edges.push(line)
}

Graph.prototype.updateLayout = function() {
	for (i in this.vertices) {
		this.forcex[i] = 0;
		this.forcey[i] = 0;
		for (j in this.vertices) {
			if( i !== j ) {
				// using rectangle's center, bounding box would be better
				var deltax = this.vertices[j].x - this.vertices[i].x;
				var deltay = this.vertices[j].y - this.vertices[i].y;
				var d2 = deltax * deltax + deltay * deltay;

				// add some jitter if distance^2 is very small
				if( d2 < 0.01 ) {
	                deltax = 0.1 * Math.random() + 0.1;
	                deltay = 0.1 * Math.random() + 0.1;
					var d2 = deltax * deltax + deltay * deltay;
                }

				// Coulomb's law -- repulsion varies inversely with square of distance
				this.forcex[i] -= (this.repulsion / d2) * deltax;
				this.forcey[i] -= (this.repulsion / d2) * deltay;

				// spring force along edges, follows Hooke's law
				if( this.vertices[i].edges[j] ) {
					var distance = Math.sqrt(d2);
					this.forcex[i] += (distance - this.spring_length) * deltax;
					this.forcey[i] += (distance - this.spring_length) * deltay;
				}
			}
		}
	}

	for (i in this.vertices) {
		// update rectangles
		this.vertices[i].x += this.forcex[i] * this.stepsize;
		this.vertices[i].y += this.forcey[i] * this.stepsize;
		
	}
	this.ctx.fillStyle = 'white'
	this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height)
	var ctx = this.ctx

	for(var i in this.vertices) {
		var v = this.vertices[i]
	//	ctx.drawImage(v.image, 0, 0)
	}

	var pulse = (new Date().getTime() % 1000) / 1000
	pulse -= 0.5
	pulse *= 2 * Math.PI
	pulse = (Math.sin(pulse) + 1) / 2

		var v = this.active
		this.ctx.beginPath();
		this.ctx.arc(this.x + v.x, this.y + v.y, v.size + 7 + pulse * 3, 
			0, 2 * Math.PI, false);
		this.ctx.fillStyle = '#E0BC1B'
		this.ctx.fill();	


	for(var i in this.edges) {
		var e = this.edges[i]
		this.ctx.beginPath();
		this.ctx.moveTo(this.x + e.a.x, this.y + e.a.y)
		this.ctx.lineTo(this.x + e.b.x, this.y + e.b.y)
		this.ctx.strokeStyle = e.color
		this.ctx.lineWidth = e.size
		this.ctx.stroke();	
	}

	for(var i in this.vertices) {
		var v = this.vertices[i]
		this.ctx.beginPath();
		this.ctx.arc(this.x + v.x, this.y + v.y, v.size, 0, 2 * Math.PI, false);
		this.ctx.fillStyle = v.color
		this.ctx.fill();	


	}

}
Graph.prototype.go = function() {
	// already running
	if (this.task) {
		return;
	}
	obj = this;
	this.iteration = 0;
	this.task = window.setInterval(function(){ obj.updateLayout(); }, 1000/60);
}
Graph.prototype.quit = function() {
	window.clearInterval(this.task);
	this.task = null;
}
