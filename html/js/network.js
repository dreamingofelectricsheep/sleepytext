

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



function synch(req) {
	if(typeof req == undefined || req == undefined) {
		req = new XMLHttpRequest()
		req.timeout = 5000
		req.ontimeout = function() {
			alert("Timeout!")
		}

		req.onreadystatechange = function() {
			if(req.readyState != 4) return;
			if(req.status == 200) {
					intransfer = []
			}
			else alert("Transfer error, code: " + req.status)
		}
	}
	
	if(typeof user != undefined && user != undefined) {		
		if(intransfer.length == 0 && queue.length > 0) {
			intransfer = queue
			queue = []

			req.open('POST', '/api/0/feed')

			req.send(transferencode(intransfer))
			
		}
	}

	setTimeout(function() { synch(req) }, 1000)

}
