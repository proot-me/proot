const proot = require('./index');
const fs = require('fs');
const path = require('path');

const testFile = 'test-file.txt';
const absoluteTestFile = path.resolve(__dirname, testFile);
const boundPath = '/tmp/bound-file.txt'; // Using /tmp directory

// 1. Create a dummy file.
fs.writeFileSync(testFile, 'This is a test file.');
console.log(`Created a temporary file: ${absoluteTestFile}`);


console.log(`\n--- Running proot to bind '${absoluteTestFile}' to '${boundPath}' and list it ---`);
const args = ['-b', `${absoluteTestFile}:${boundPath}`, 'ls', '-l', boundPath];
const prootProcess = proot(args);

prootProcess.on('exit', (code) => {
  console.log(`\nProot process exited with code ${code}.`);

  // Clean up the dummy file.
  fs.unlinkSync(testFile);
  console.log(`Cleaned up temporary file: ${testFile}`);

  if (code === 0) {
    console.log('\n✅ Example ran successfully!');
  } else {
    console.error('\n❌ Example failed. The proot command returned a non-zero exit code.');
  }
});
