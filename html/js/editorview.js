
function entereditorview(activebranch) {
	view = {}
	view.editor = document.createElement("textarea")
	view.editor.setAttribute('autocomplete', 'off')
	view.editor.setAttribute('id', 'editor')


	view.graph = document.createElement('div')
	view.graph.setAttribute('id', 'graphbutton')
	view.graph.innerHTML = 'graph'

	view.branchbtn = document.createElement('div')
	view.branchbtn.setAttribute('id', 'branchbutton')
	view.branchbtn.innerHTML = 'branch'

	view.slider = new slider(body, function(p) {
		var go = Math.floor(p * view.history.steps.length)

		view.history.go(go)
		view.editor.last = view.editor.value = view.history.render
	})

	view.branch = activebranch == undefined ? new branch() : activebranch
	view.history = view.branch.gethistory()

	view.history.render = view.editor.last = view.editor.value = view.branch.render
	view.history.position = view.history.steps.length


	view.editor.oninput = function(e) {
		if(view.history.position != view.history.steps.length) {
			branchout()
		}

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
		view.branch.present.push(change)
		view.history.steps.push(change)
		view.history.position = view.history.steps.length

		view.history.render = view.branch.render = view.editor.last = text
	}
	
	function branchout() {
		var past = undefined;
		var i = view.branch

		var pointer = view.history.position
	
		
		while(true) {
			if(i.past == undefined) {
				past = { branch: i, at: pointer }
				break;
			}
			if(i.past.at > pointer) {
				i = i.past.branch
			} else {
				past = { branch: i, at: pointer }
				break;
			}
		}
		
		view.branch = new branch(past)
		view.history = view.branch.gethistory()
		view.history.position = view.history.steps.length

		view.slider.to(1, true)
	}


	view.branchbtn.onclick = function() {
		branchout()
	}

	view.graph.onclick = function() {
		var lastbranch = view.branch
		view.leave()
		entergraphview(lastbranch)
	}
	
	body.appendChild(view.editor)
	body.appendChild(view.graph)
	body.appendChild(view.branchbtn)

	view.editor.focus()

	view.leave = function() {
		var list = [view.editor, view.branchbtn, view.graph]
		for(var i in list)
			body.removeChild(list[i])

		view.slider.despawn()
		view = undefined
	}
}



