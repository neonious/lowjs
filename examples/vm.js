var vm = require('vm');

var script = vm.createScript('while(true) {throw new Error("aa");}');
var context = vm.createContext();

script.runInContext(context, {timeout: 5000, breakOnSigint: true});
