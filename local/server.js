var http = require("http");
var url = require("url");
var path = require("path");
var fs = require("fs");

var basePath = process.argv[2] || process.cwd();
var port = process.argv[3] || 5350;

http.createServer(function(request, response)
{
	var uri = url.parse(request.url).pathname
	var filename = path.join(basePath, uri);

	fs.exists(filename, function(exists)
	{
		if(!exists)
		{
			if(uri !== "/index.html") {
				response.writeHead(302, {"Location": "/index.html"});
				response.end();
				return;
			}

			response.writeHead(404, {"Content-Type": "text/plain"});
			response.write("404 Not Found\n");
			response.end();
			return;
		}

		if (fs.statSync(filename).isDirectory())
			filename += 'index.html';

		var readStream = fs.createReadStream(filename);

		readStream.on('open', function ()
		{
			if(filename.length - 5 == filename.indexOf('.html'))
				response.writeHead(200, {"Content-Type": "text/html"});
			else
				response.writeHead(200, {"Content-Type": "application/octet-stream"});

			readStream.pipe(response);
		});

		readStream.on('end', function()
		{
			response.end();
		});

		readStream.on('error', function(err)
		{
			response.writeHead(500, {"Content-Type": "text/plain"});
			response.write(err + "\n");
			response.end();
		});
	});

}).listen(parseInt(port, 10));

console.log('Serving directory ' + basePath + ' on port ' + port);
