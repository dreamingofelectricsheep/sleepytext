var queue = [], intransfer = []
var branches = {}
var docs = {}

function branch(id, parent, pos, document, data) {
	this.id = id
	this.parent = parent
	this.pos = pos
	this.document = document
	this.data = data == undefined ? [] : data
	this.time = new Date().getTime()
	this.render = ''

	branches[id] = this

	if(docs[document] == undefined) docs[document] = []

	docs[document].push(this)
}

branch.prototype = {
	getlength: function() {
		return this.gethistory().length
	},
	gethistory: function() {
		var past = this.getprecedinghistory()
		return new history(past.concat(this.data))
	},
	getprecedinghistory: function() {
		var result = []

		if(this.parent != 0) {
			result = branches[this.parent].getprecedinghistory()
			result = result.concat(branches[this.parent].data.slice(0,
				this.pos - result.length))
		}

		return result
	}
}

