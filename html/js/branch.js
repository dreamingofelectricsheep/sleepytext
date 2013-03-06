var queue = [], intransfer = []
var branches = []
var docs = []

function branch(doc, past) {
	this.past = past
	this.present = []
	this.time = new Date().getTime()
	this.render = ''
	this.doc = doc

	branches.push(this)
	if(docs[doc] == undefined) docs[doc] = []
	
	docs[doc].push(this)
}

branch.prototype = {
	getlength: function() {
		var past = 0
		if(this.past) past = this.past.at
		return past + this.present.length 
	},
	gethistory: function() {
		var past = this.getprecedinghistory()
		return new history(past.concat(this.present))
	},
	getprecedinghistory: function() {
		var result = []

		if(this.past) {
			result = this.past.branch.getprecedinghistory()
			result = result.concat(this.past.branch.present.slice(0,
				this.past.at - result.length))
		}

		return result
	}
}

