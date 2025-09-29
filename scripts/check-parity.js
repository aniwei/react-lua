#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

const ROOT_DIR = path.resolve(__dirname, '..');
const DEFAULT_CPP_ROOT = path.join(ROOT_DIR, 'packages', 'ReactCpp', 'src');

function walk(dir, predicate, acc = []) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    if (entry.name === 'node_modules') continue;
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walk(fullPath, predicate, acc);
    } else if (predicate(entry.name)) {
      acc.push(fullPath);
    }
  }
  return acc;
}

function parseArgs(argv) {
  const options = {
    cppRoot: DEFAULT_CPP_ROOT,
    verbose: false,
    reportPath: null,
  };

  for (let i = 2; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === '--verbose') {
      options.verbose = true;
      continue;
    }
    if (arg.startsWith('--cppRoot=')) {
      const value = arg.split('=')[1];
      if (value) {
        options.cppRoot = path.resolve(ROOT_DIR, value);
      }
      continue;
    }
    if (arg.startsWith('--report=')) {
      const value = arg.split('=')[1];
      if (value) {
        options.reportPath = path.resolve(ROOT_DIR, value);
      }
      continue;
    }
  }

  return options;
}

function relativeFromRoot(filePath) {
  return path.relative(ROOT_DIR, filePath);
}

function collectExpectSnapshots(cppRoot) {
  if (!fs.existsSync(cppRoot)) {
    throw new Error(`C++ root does not exist: ${cppRoot}`);
  }
  return walk(cppRoot, name => name.endsWith('.expect.json'));
}

function readJSON(filePath) {
  const raw = fs.readFileSync(filePath, 'utf8');
  try {
    return JSON.parse(raw);
  } catch (error) {
    throw new Error(`Failed to parse JSON ${filePath}: ${error.message}`);
  }
}

function ensureFunctionSignature(content, functionName) {
  const signaturePattern = new RegExp(`\\b${functionName}\\s*\\(`);
  return signaturePattern.test(content);
}

function verifySnapshot(snapshotPath, options) {
  const basePath = snapshotPath.replace(/\.expect\.json$/, '');
  const headerPath = `${basePath}.h`;
  const sourcePath = `${basePath}.cpp`;

  const issues = [];
  const snapshot = readJSON(snapshotPath);

  if (!fs.existsSync(headerPath)) {
    issues.push(`Missing header file: ${relativeFromRoot(headerPath)}`);
  }
  if (!fs.existsSync(sourcePath)) {
    issues.push(`Missing source file: ${relativeFromRoot(sourcePath)}`);
  }

  let headerContent = ''; let sourceContent = '';
  if (fs.existsSync(headerPath)) {
    headerContent = fs.readFileSync(headerPath, 'utf8');
  }
  if (fs.existsSync(sourcePath)) {
    sourceContent = fs.readFileSync(sourcePath, 'utf8');
  }

  for (const entry of snapshot.exports) {
    if (entry.kind !== 'function') {
      continue;
    }
    const name = entry.name;
    const hasHeader = ensureFunctionSignature(headerContent, name);
    const hasSource = ensureFunctionSignature(sourceContent, name);
    if (!hasHeader) {
      issues.push(`Missing declaration for ${name} in header ${relativeFromRoot(headerPath)}`);
    }
    if (!hasSource) {
      issues.push(`Missing definition for ${name} in source ${relativeFromRoot(sourcePath)}`);
    }
  }

  const passed = issues.length === 0;
  if (options.verbose && passed) {
    console.log(`✅ ${relativeFromRoot(snapshotPath)}`);
  }

  return { passed, issues };
}

function main() {
  const options = parseArgs(process.argv);
  const snapshotPaths = collectExpectSnapshots(options.cppRoot);

  if (snapshotPaths.length === 0) {
    console.log('No .expect.json snapshots found.');
    if (options.reportPath) {
      writeReport(options.reportPath, [], []);
    }
    process.exit(0);
  }

  let totalIssues = 0;
  const failures = [];
  const successes = [];

  for (const snapshotPath of snapshotPaths) {
    const { passed, issues } = verifySnapshot(snapshotPath, options);
    if (passed) {
      successes.push({ snapshotPath });
    } else {
      totalIssues += issues.length;
      failures.push({ snapshotPath, issues });
    }
  }

  if (options.reportPath) {
    writeReport(options.reportPath, successes, failures);
  }

  if (totalIssues === 0) {
    console.log(`✨ Parity check passed for ${snapshotPaths.length} snapshot(s).`);
    process.exit(0);
  }

  console.error(`❌ Parity check failed. Total issues: ${totalIssues}`);
  for (const { snapshotPath, issues } of failures) {
    console.error(`- ${relativeFromRoot(snapshotPath)}`);
    for (const issue of issues) {
      console.error(`  • ${issue}`);
    }
  }
  process.exit(1);
}

function writeReport(reportPath, successes, failures) {
  const lines = [];
  const now = new Date().toISOString();
  const total = successes.length + failures.length;
  lines.push('# React Parity Report');
  lines.push('');
  lines.push(`- Generated: ${now}`);
  lines.push(`- Total snapshots: ${total}`);
  lines.push(`- Pass: ${successes.length}`);
  lines.push(`- Fail: ${failures.length}`);
  lines.push('');
  if (total === 0) {
    lines.push('No snapshot data available.');
  } else {
    lines.push('| Snapshot | Status | Notes |');
    lines.push('| --- | --- | --- |');
    successes.forEach(({ snapshotPath }) => {
      lines.push(`| ${relativeFromRoot(snapshotPath)} | ✅ Pass | - |`);
    });
    failures.forEach(({ snapshotPath, issues }) => {
      const note = issues.map(issue => issue.replace(/\|/g, '\\|')).join('<br/>');
      lines.push(`| ${relativeFromRoot(snapshotPath)} | ❌ Fail | ${note} |`);
    });
  }

  const outDir = path.dirname(reportPath);
  fs.mkdirSync(outDir, { recursive: true });
  fs.writeFileSync(reportPath, `${lines.join('\n')}\n`);
}

main();
