const { spawn } = require('child_process');
const path = require('path');

// Resolve the path to the proot executable, which should be in src/ after compilation.
const prootExecutable = path.resolve(__dirname, 'src', 'proot');

/**
 * Executes a command within the proot environment.
 * @param {string[]} args - An array of arguments to pass to proot.
 * @returns {import('child_process').ChildProcess} The spawned child process.
 */
function proot(args) {
  if (!Array.isArray(args)) {
    throw new Error('Arguments must be provided as an array of strings.');
  }

  const child = spawn(prootExecutable, args, {
    stdio: 'inherit' // Pipe stdin, stdout, stderr to the parent process
  });

  child.on('error', (err) => {
    console.error('Failed to start proot process:', err);
  });

  return child;
}

module.exports = proot;
