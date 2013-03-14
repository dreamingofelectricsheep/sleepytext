
function entereditorview(activebranch) {
	view = {}
	view.editor = tags.textarea({ 
		autocomplete: 'off',
		id: 'editor' })


	view.graph = tags.div({ id: 'graphbutton', class: 'button' }, 'graph')
	view.branchbtn = tags.div({ id: 'branchbutton', class: 'button' }, 'branch')
	view.userbtn = tags.div({ id: 'userbtn', class: 'button' }, 'account')

	view.slider = new slider(body, function(p) {
		var go = Math.floor(p * view.history.steps.length)

		view.history.go(go)
		view.editor.last = view.editor.value = view.history.render
	})

	view.branch = activebranch == undefined ? 
		new branch(1, 0, 0, 1) : activebranch
	view.history = view.branch.gethistory()

	view.history.render = view.editor.last = view.editor.value = view.branch.render
	view.history.position = view.history.steps.length

	view.buttonbox = tags.div({ id: 'buttonbox' })


	view.editor.oninput = function(e) {
		if(view.history.position != view.history.steps.length) {
			branchout()
		}
		
		view.editor.style.height = view.editor.scrollHeight + 'px'

		var text = e.target.value
		var last = view.editor.last
		var i = 0, j = 0

		var min = Math.min(last.length, text.length)

		for(; i < min; i++) {
			if(last[i] != text[i]) break; }

		for(; min - j > i; j++) {
			if(last[last.length-j-1] != text[text.length-j-1]) break; }

		


		var ls = last.slice(i, last.length -j)
		var ts = text.slice(i, text.length -j)
		var change = {
			time: new Date().getTime(),
			pos: i,
			last: ls,
			now: ts }

		queue.push(change)
		view.branch.data.push(change)
		view.history.steps.push(change)
		view.history.position = view.history.steps.length

		view.history.render = view.branch.render = view.editor.last = text
	}
	
	function branchout() {
		var past = undefined;
		var i = view.branch

		var pointer = view.history.position
	
	
		view.branch = new branch(Math.floor(Math.random()*10000),
			view.branch.id, pointer, view.branch.doc)

		view.history = view.branch.gethistory()
		view.history.position = view.history.steps.length

		view.slider.to(1, true)
	}


	view.branchbtn.onclick = function() {
		branchout()
	}

	view.userbtn.onclick = function() {
		view.leave()
		enteraccountview()
	}

	view.graph.onclick = function() {
		var lastbranch = view.branch
		view.leave()
		entergraphview(lastbranch)
	}
	
	body.appendChild(view.editor)
	view.buttonbox.appendChild(view.graph)
	view.buttonbox.appendChild(view.branchbtn)
	view.buttonbox.appendChild(view.userbtn)
	body.appendChild(view.buttonbox)

	view.editor.focus()

	view.leave = function() {
		while(body.firstChild != undefined)
			body.removeChild(body.firstChild)

		view = undefined
	}
}



