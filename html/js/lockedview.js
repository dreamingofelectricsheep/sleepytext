function enterlockedview() {
	view = {}
	body.classList.add('locked')
	body.innerHTML = 'Locked!'

	view.leave = function() {
		body.innerHTML = ''
		body.classList.remove('locked')
		document.onmouseup = undefined
	}

	document.onmouseup = function() {
		request('/api/0/lock', undefined, function() {
			view.leave()
			entereditorview()
		}, function() { alert('Could not lock for some reason') })
	}
}
