var os = require('os');

function log_stdout(input = "") {
  process.stdout.write(input + os.EOL);
}
log_stdout("exec testing....");

var coroutine = require("coroutine");

process.on("SIGTERM", () => {
  log_stdout("killed");
  process.exit(1);
});

for (var i = 0; i < 2; i++) {
  coroutine.sleep(1000);
  log_stdout(new Date());
  console.print("console.print....");
  log_stdout();
}

process.exit();
log_stdout("not output");