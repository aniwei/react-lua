#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const parser = require('@babel/parser');
const traverse = require('@babel/traverse').default;

const ROOT_DIR = path.resolve(__dirname, '..');
const DEFAULT_OUT_DIR = path.join(ROOT_DIR, 'packages', 'ReactCpp', 'src');

/**
 * Minimal CLI parsing helper.
 */
function parseArgs(argv) {
  const result = {
    source: null,
    outDir: DEFAULT_OUT_DIR,
    force: false,
    dryRun: false,
    verbose: false,
  };

  for (let i = 2; i < argv.length; ++i) {
    const arg = argv[i];
    if (!arg.startsWith('--')) {
      continue;
    }

    if (arg === '--force') {
      result.force = true;
      continue;
    }
    if (arg === '--dry-run') {
      result.dryRun = true;
      continue;
    }
    if (arg === '--verbose') {
      result.verbose = true;
      continue;
    }

    const [key, value] = arg.split('=');
    if (!value) {
      continue;
    }
    switch (key) {
      case '--source':
        result.source = path.resolve(ROOT_DIR, value);
        break;
      case '--outDir':
        result.outDir = path.resolve(ROOT_DIR, value);
        break;
      default:
        break;
    }
  }

  return result;
}

function die(message) {
  console.error(`translate-react: ${message}`);
  process.exitCode = 1;
  process.exit();
}

function ensureReactMainRelative(sourcePath) {
  const reactMainDir = path.join(ROOT_DIR, 'react-main');
  if (!fs.existsSync(reactMainDir)) {
    die('react-main mirror not found. Expected directory: react-main/');
  }
  const relative = path.relative(reactMainDir, sourcePath);
  if (relative.startsWith('..')) {
    die(`source must live under react-main/. Received: ${sourcePath}`);
  }
  return { reactMainDir, relative };
}

function computeCppPaths(relativeSource, outDir) {
  const parts = relativeSource.split(path.sep);
  const pkgIndex = parts.indexOf('packages');
  const srcIndex = parts.indexOf('src');

  if (pkgIndex === -1 || srcIndex === -1 || srcIndex <= pkgIndex + 1) {
    die(`cannot infer package/src layout from: ${relativeSource}`);
  }

  const packageName = parts[pkgIndex + 1];
  const subPathParts = parts.slice(srcIndex + 1);
  if (subPathParts.length === 0) {
    die(`missing file segment after src/: ${relativeSource}`);
  }

  const fileName = subPathParts.pop();
  const baseName = fileName
    .replace(/\.(jsx?|ts|tsx)$/i, '')
    .replace(/\.new$/i, '');

  const destDir = path.join(outDir, packageName, ...subPathParts);
  const headerPath = path.join(destDir, `${baseName}.h`);
  const sourcePath = path.join(destDir, `${baseName}.cpp`);
  const snapshotPath = path.join(destDir, `${baseName}.expect.json`);

  return { destDir, headerPath, sourcePath, snapshotPath, baseName, packageName, subPath: subPathParts, fileName };
}

function parseReactSource(sourceCode, sourceFilename) {
  const ast = parser.parse(sourceCode, {
    sourceType: 'module',
    sourceFilename,
    allowReturnOutsideFunction: true,
    plugins: [
      'jsx',
      'flow',
      'flowComments',
      'classProperties',
      'classPrivateMethods',
      'classPrivateProperties',
      'dynamicImport',
      'optionalChaining',
      'nullishCoalescingOperator',
      'numericSeparator',
      'exportNamespaceFrom',
      'logicalAssignment',
      'topLevelAwait',
    ],
  });

  const declarations = new Map();

  traverse(ast, {
    Program(path) {
      path.get('body').forEach(child => {
        const node = child.node;
        if (node.type === 'FunctionDeclaration' && node.id) {
          declarations.set(node.id.name, {
            kind: 'function',
            loc: node.loc,
          });
        }
        if (node.type === 'VariableDeclaration') {
          node.declarations.forEach(declarator => {
            if (declarator.id.type !== 'Identifier') {
              return;
            }
            const name = declarator.id.name;
            const init = declarator.init;
            if (!init) {
              declarations.set(name, {
                kind: 'variable',
                loc: declarator.loc,
              });
              return;
            }
            if (
              init.type === 'FunctionExpression' ||
              init.type === 'ArrowFunctionExpression'
            ) {
              declarations.set(name, {
                kind: 'function',
                loc: declarator.loc ?? init.loc,
              });
            } else {
              declarations.set(name, {
                kind: 'variable',
                loc: declarator.loc,
              });
            }
          });
        }
        if (node.type === 'ClassDeclaration' && node.id) {
          declarations.set(node.id.name, {
            kind: 'class',
            loc: node.loc,
          });
        }
      });
    },
  });

  const exports = [];
  const notes = [];

  traverse(ast, {
    ExportNamedDeclaration(path) {
      const { declaration, specifiers } = path.node;
      if (declaration) {
        if (declaration.type === 'FunctionDeclaration' && declaration.id) {
          exports.push({
            name: declaration.id.name,
            kind: 'function',
            loc: declaration.loc,
          });
          return;
        }
        if (declaration.type === 'VariableDeclaration') {
          declaration.declarations.forEach(declarator => {
            if (declarator.id.type !== 'Identifier') {
              return;
            }
            const name = declarator.id.name;
            const init = declarator.init;
            const kind = init && (init.type === 'FunctionExpression' || init.type === 'ArrowFunctionExpression')
              ? 'function'
              : 'variable';
            exports.push({ name, kind, loc: declarator.loc });
          });
          return;
        }
        if (declaration.type === 'ClassDeclaration' && declaration.id) {
          exports.push({ name: declaration.id.name, kind: 'class', loc: declaration.loc });
          return;
        }
      }

      specifiers.forEach(specifier => {
        const exportedName = specifier.exported.name;
        const localName = specifier.local.name;
        const decl = declarations.get(localName);
        if (decl) {
          exports.push({ name: exportedName, kind: decl.kind, loc: decl.loc });
        } else {
          notes.push(`Unable to locate declaration for exported symbol ${exportedName}`);
        }
      });
    },
    ExportDefaultDeclaration(path) {
      const { node } = path;
      let name = 'default';
      let kind = 'unknown';
      if (node.declaration.type === 'Identifier') {
        name = node.declaration.name;
        const decl = declarations.get(name);
        if (decl) {
          kind = decl.kind;
        }
      } else if (
        node.declaration.type === 'FunctionDeclaration' &&
        node.declaration.id
      ) {
        name = node.declaration.id.name;
        kind = 'function';
      } else if (
        node.declaration.type === 'ClassDeclaration' &&
        node.declaration.id
      ) {
        name = node.declaration.id.name;
        kind = 'class';
      }
      exports.push({ name, kind, loc: node.loc, isDefault: true });
    },
  });

  return { exports, notes };
}

function formatLocation(loc) {
  if (!loc) {
    return 'lines ?-?';
  }
  return `lines ${loc.start.line}-${loc.end.line}`;
}

function buildHeader(baseName, sourceRelative, exportSummaries) {
  const exports = exportSummaries.filter(item => item.kind === 'function');
  const others = exportSummaries.filter(item => item.kind !== 'function');

  const lines = [];
  lines.push('#pragma once');
  lines.push('');
  lines.push('// Auto-generated by scripts/translate-react.js');
  lines.push(`// Source: react-main/${sourceRelative}`);
  lines.push('');
  lines.push('namespace react {');
  lines.push('');

  if (exports.length === 0) {
    lines.push(`// TODO: No function exports detected in ${baseName}.`);
  } else {
    exports.forEach(entry => {
      const comment = formatLocation(entry.loc);
      lines.push(`// ${comment}`);
      if (entry.isDefault) {
        lines.push('// NOTE: Default export translated as free function stub.');
      }
      lines.push(`void ${entry.name}();`);
      lines.push('');
    });
  }

  if (others.length > 0) {
    lines.push('// TODO: The following exports require manual translation:');
    others.forEach(entry => {
      lines.push(`// - ${entry.name} (${entry.kind}, ${formatLocation(entry.loc)})`);
    });
    lines.push('');
  }

  lines.push('} // namespace react');
  lines.push('');
  return lines.join('\n');
}

function buildSource(baseName, headerPath, sourceRelative, exportSummaries) {
  const exports = exportSummaries.filter(item => item.kind === 'function');
  const lines = [];
  lines.push(`#include "${path.basename(headerPath)}"`);
  lines.push('');
  lines.push('// Auto-generated by scripts/translate-react.js');
  lines.push(`// Source: react-main/${sourceRelative}`);
  lines.push('');
  lines.push('namespace react {');
  lines.push('');

  if (exports.length === 0) {
    lines.push(`// TODO: Populate ${baseName}.cpp with translated logic.`);
  } else {
    exports.forEach(entry => {
      const comment = formatLocation(entry.loc);
      if (entry.isDefault) {
        lines.push('// NOTE: Default export translated as free function stub.');
      }
      lines.push(`// ${comment}`);
      lines.push(`void ${entry.name}() {`);
      lines.push('  // TODO: Translate implementation.');
      lines.push('}');
      lines.push('');
    });
  }

  lines.push('} // namespace react');
  lines.push('');
  return lines.join('\n');
}

function writeFileIfNeeded(targetPath, contents, options) {
  const { dryRun, force, verbose } = options;
  const exists = fs.existsSync(targetPath);
  if (exists && !force) {
    if (verbose) {
      console.log(`➡️  skip existing ${path.relative(ROOT_DIR, targetPath)}`);
    }
    return;
  }

  if (dryRun) {
    console.log(`--- ${path.relative(ROOT_DIR, targetPath)} ---`);
    console.log(contents);
    console.log('--- end ---');
    return;
  }

  fs.mkdirSync(path.dirname(targetPath), { recursive: true });
  fs.writeFileSync(targetPath, contents);
  if (verbose) {
    console.log(`✅ wrote ${path.relative(ROOT_DIR, targetPath)}`);
  }
}

function writeSnapshot(snapshotPath, payload, options) {
  const { dryRun, force, verbose } = options;
  const exists = fs.existsSync(snapshotPath);
  if (exists && !force) {
    if (verbose) {
      console.log(`➡️  skip existing ${path.relative(ROOT_DIR, snapshotPath)}`);
    }
    return;
  }
  const json = JSON.stringify(payload, null, 2);
  if (dryRun) {
    console.log(`--- ${path.relative(ROOT_DIR, snapshotPath)} ---`);
    console.log(json);
    console.log('--- end ---');
    return;
  }
  fs.mkdirSync(path.dirname(snapshotPath), { recursive: true });
  fs.writeFileSync(snapshotPath, `${json}\n`);
  if (verbose) {
    console.log(`✅ wrote ${path.relative(ROOT_DIR, snapshotPath)}`);
  }
}

function main() {
  const args = parseArgs(process.argv);

  if (!args.source) {
    die('missing --source=<path to react-main file>');
  }

  if (!fs.existsSync(args.source)) {
    die(`source file does not exist: ${args.source}`);
  }

  const { relative: sourceRelative } = ensureReactMainRelative(args.source);
  const { destDir, headerPath, sourcePath, snapshotPath, baseName } = computeCppPaths(
    sourceRelative,
    args.outDir,
  );

  const sourceCode = fs.readFileSync(args.source, 'utf8');
  const { exports, notes } = parseReactSource(sourceCode, sourceRelative);

  const headerContents = buildHeader(baseName, sourceRelative, exports);
  const sourceContents = buildSource(baseName, headerPath, sourceRelative, exports);
  const snapshotPayload = {
    source: `react-main/${sourceRelative}`,
    generatedAt: new Date().toISOString(),
    exports: exports.map(entry => ({
      name: entry.name,
      kind: entry.kind,
      isDefault: Boolean(entry.isDefault),
      loc: entry.loc
        ? {
            start: { line: entry.loc.start.line, column: entry.loc.start.column },
            end: { line: entry.loc.end.line, column: entry.loc.end.column },
          }
        : null,
    })),
    notes,
  };

  if (args.verbose) {
    console.log('translate-react summary:', snapshotPayload);
  }

  writeFileIfNeeded(headerPath, headerContents, args);
  writeFileIfNeeded(sourcePath, sourceContents, args);
  writeSnapshot(snapshotPath, snapshotPayload, args);

  if (!args.dryRun) {
    console.log(`✨ Generated stubs for react-main/${sourceRelative}`);
    console.log(`    → ${path.relative(ROOT_DIR, headerPath)}`);
    console.log(`    → ${path.relative(ROOT_DIR, sourcePath)}`);
    console.log(`    → ${path.relative(ROOT_DIR, snapshotPath)}`);
  }
}

main();
