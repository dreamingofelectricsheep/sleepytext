function isloggedin() {
	var cookies = document.cookie.split(';')
		.map(function(c) { 
			var co = c.trim().split('=')
			return { name: co[0], value: co[1] } })

	var o = {}
	for(var i in cookies) { o[cookies[i].name] = cookies[i].value }
	cookies = o

	if(cookies.session == undefined || cookies.session == '0') return false
	return true
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

function enteruserview() {
	view = {}
	
	view.logout = tags.div({ id: 'logout', class: 'button' }, 'Log out')
	view.logout.onclick = function() {
		document.cookie = 'session=0'
		view.leave()
		enteraccountview()
	}

	view.createbtn = tags.div({ id: 'docbtn', class: 'button' }, 'New document')
	
	request('/api/0/branches', undefined, function(req) {
		var t = req.responseText
	
		t = t.split('\n')
		for(var i in t) {
			var r = t[i].split(' ')
			body.appendChild(tags.div({ class: 'document-entry' }, r[0]))
		}
	}, function() { alert('error loading documents') })


	view.createbtn.onclick = function() {
		request('/api/0/newbranch', 'test\n1 2 3 4 5', function(req) {
			view.leave()
			enteraccountview()
		}, function() { alert("Error creating a new branch!") })
	}
			
	

	body.appendChild(view.createbtn)
	body.appendChild(view.logout)

	view.leave = function() {
		while(body.firstChild != undefined)
			body.removeChild(body.firstChild)
	}	

}

function enteraccountview() {
	view = {}

	if(isloggedin() == true)
		enteruserview()
	else enterloginview()


}
