function isloggedin() {
	if(document.cookie.indexOf('session') == -1) return false
	return true;
}

function enterloginview() {
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
		var user = {
			login: view.username.value,
			password: view.password.value
		}

		request('/api/0/user', user.login + '\n' + user.password,
			function() {
				view.leave()
				entereditorview()
			}, function() {
				alert('Could not create such user') })
	}
	
	view.login.onclick = function() {
		var user = {
			login: view.username.value,
			password: view.password.value
		}

		request('/api/0/login', user.login + '\n' + user.password,
			function() {
				view.leave()
				entereditorview()
			}, function() { alert("Wrong login") })	
	}
	
}

function enteraccountview() {
	view = {}

	
	enterloginview()


}
