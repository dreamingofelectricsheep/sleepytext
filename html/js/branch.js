var queue = [], intransfer = []
var branches = {}
var docs = {}

// Expected values: id, parent, pos, document
// Optional: data
function branch(o) {
	for(var i in o)
	{
		this[i] = o[i]
	}

	this.data = o.data == undefined ? [] : data
	this.time = new Date().getTime()
	this.render = ''

	branches[this.id] = this

	if(docs[this.document] == undefined) docs[this.document] = []

	docs[this.document].push(this)
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

