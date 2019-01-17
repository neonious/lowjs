var express = require("express");
var app = express();
var bodyParser = require("body-parser");
var chai = require("chai");
var expect = chai.expect;
var should = chai.should();

app.use(bodyParser.json());

app.get("/sendFile", function(req, res) {
  res.sendFile(__filename);
});

app.get("/sendText", function(req, res) {
  res.send("Hello World!");
});

app.get("/redirect", function(req, res) {
  res.redirect("/sendText");
});

app.get("/badStatus", function(req, res) {
  res.sendStatus(500);
});

app.get("/", function(req, res) {
  res.send("This is the root!");
});

app.get("/muchData", function(req, res) {
  var filled = 0;
  var int = setInterval(function() {
    var add = 500000;
    filled += add;
    var buf = new Buffer(add).fill(0);
    res.write(buf);
    if (filled === 10000000) {
      res.end();
      clearInterval(int);
    }
  }, 20);
});

app.post("/json", function(req, res) {
  const obj = req.body;
  res.json(obj);
});

app.listen(3000, function() {
  console.log("Example app listening on port 3000!");

  const request = require("request");

  request("http://www.google.com", function(error, response, body) {
    assertStatus(error, response, 200);
    should.equal(body[0], "<");
  });
  request(
    "http://localhost:3000/json",
    { method: "POST", json: true, body: { test: "test" } },
    function(error, response, body) {
      assertStatus(error, response, 200);
      should.equal(body.test, "test");
    }
  );
  function assertStatus(error, response, status) {
    should.not.exist(error);
    should.equal(response && response.statusCode, status);
  }
  request("http://localhost:3000/sendText", function(error, response, body) {
    assertStatus(error, response, 200);
    should.equal(body, "Hello World!");
  });
  request("http://localhost:3000/sendFile", function(error, response, body) {
    assertStatus(error, response, 200);
    expect(body.startsWith("var express")).to.be.true;
  });
  request("http://localhost:3000/redirect", function(error, response, body) {
    assertStatus(error, response, 200);
    should.equal(body, "Hello World!");
  });
  request("http://localhost:3000/badStatus", function(error, response, body) {
    assertStatus(error, response, 500);
  });
  var buf = new Buffer([]);
  request("http://localhost:3000/muchData", function(error, response, body) {
    assertStatus(error, response, 200);
    should.equal(buf.byteLength, 10000000);
  }).on("data", function(chunk) {
    buf = Buffer.concat([buf, chunk]);
  });
});
