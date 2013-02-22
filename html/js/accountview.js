var user;

function request(addr, data, success, failure) {
	var req = new XMLHttpRequest()
	req.open('POST', addr)
	req.timeout = 5000
	req.ontimeout = function() { alert('Timeout') }

	req.onreadystatechange = function() {
		if(req.readyState != 4) return
		if(req.status != 200 && typeof failure != undefined) 
			failure(req.status)
		if(typeof success != undefined) success()
	}
	
	req.send(data)
}


function makelogin(user, success, failure) {
	request('/api/0/login', user.login + '\n' + user.password, success, failure)
}


function trytologin() {
	if(localStorage.user != undefined) {	
		var tryuser = JSON.parse(localStorage.user)
		makelogin(tryuser, function() { user = tryuser }, 
			function() { 
				localStorage.removeItem('user')
				alert("Could not log in!") })
	}
}


function enternoaccountview() {
	view = {
		username: tags.input({ id: 'username', type: 'text', 
			placeholder: 'john@example.com' }),
		password: tags.input({ id: 'password', type: 'password',
			placeholder: 'Type in a long password here!' }),
		login: tags.div({ id: 'login', class: 'button' }, 'Log in'),
		register: tags.div({ id: 'register', class: 'button' }, 'Register')
	}

	var dom = []
	for(var i in view) dom.push(i)

	for(var i in dom) body.appendChild(view[dom[i]])

	view.leave = function() {
		for(var i in dom)
			body.removeChild(view[dom[i]])
	}

	view.register.onclick = function() {
		var tryuser = {
			login: view.username.value,
			password: view.password.value
		}

		request('/api/0/user', tryuser.login + '\n' + tryuser.password,
			function() {
				user = tryuser
				view.leave()
				entereditorview()
			}, function() {
				alert('Could not create such user') })
	}
	
	view.login.onclick = function() {
		var tryuser = {
			login: view.username.value,
			password: view.password.value
		}

		makelogin(tryuser, function() {
			user = tryuser
			view.leave()
			entereditorview()
		}, function() { alert("Wrong login") })	
	}
	
}

function enteraccountview() {
	view = {}

	
	enternoaccountview()


}
