var request = new XMLHttpRequest()
request.timeout = 5000
request.ontimeout = function() {
	alert("Timeout!")
}

function encode_int32(n) {
	var bytes = new Uint8Array(4)

	for(var i = 0; i < 4; i++)
		bytes[3 - i] = (n >> (8*i)) % 256

	return bytes
}

function transferencode(list) {
	var pieces = []

	pieces.push('b')
	pieces.push(encode_int32(1337))

	for(var i in list) {
		var c = list[i]

		var type = 'i'

		var utf8 = [c.last, c.now]
		for(var i in utf8) utf8[i] = new Blob([utf8[i]])

		var data = [c.pos, utf8[0].size, utf8[1].size]
		for(var i in data) data[i] = encode_int32(data[i])


		pieces = pieces.concat([type], data, utf8)
	}

	return new Blob(pieces)
}

request.onreadystatechange = function() {
	if(request.readyState != 4) return
	if(request.status != 200) {
		alert("Login failed!")
		return
	}
		


	request.onreadystatechange = function() {
		if(request.readyState != 4) return;
		if(request.status == 200) {
				intransfer = []
		}
		else alert("Transfer error, code: " + request.status)
	}
	

	function update() {
		if(intransfer.length == 0 && queue.length > 0) {
			intransfer = queue
			queue = []

			request.open('POST', '/api/0/feed')

			request.send(transferencode(intransfer))
			
		}
	}

	setInterval(update, 1000)

}

var user = localStorage.user

if(user == undefined) {
	request.open('POST', '/api/0/user')
	user = {
		login: Math.random() * 999999999999,
		password: Math.random() * 99999999999
	}

	request.send(user.login + '\n' + user.password)
	localStorage.user = JSON.stringify(user)
} else {
	user = JSON.parse(user)
	request.open('POST', '/api/0/login')
	
	request.send(user.login + '\n' + user.password)
}

