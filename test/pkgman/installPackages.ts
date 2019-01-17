import { getOptimizedModules } from "../../lowrmt-common/src/pkgman/npm/install";
import { join } from "path";
import { toZip } from "../../lowrmt-common/src/pkgman/files/toZip";
import { writeNodeModulesFromZip } from "../../lowrmt-common/src/pkgman/files/writeNodeModulesFromZip";

async function main() {
  const modules = await getOptimizedModules({
    packages: new Set(["request", "express", "body-parser"]),
    path: join(__dirname, "node_modules_unoptimized")
  });

  const zipFile = await toZip(modules);
  await writeNodeModulesFromZip({
    zipFile,
    path: join(__dirname, "node_modules")
  });
}

main();
