// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require("fs");
const getRegexes = require("./common/regexes");

module.exports = (github, context) => {
  const linesAfterEdit = [];
  const lines = context.payload.issue.body.split("\r\n");
  const regexes = getRegexes(null, null, null);

  lines.forEach((line) => {
    regexes.forEach((test) => {
      if (test.regex.test(line)) {
        linesAfterEdit.push(line);
      }
    });
  });

  if (linesAfterEdit.length > 0) {
    github.issues.update({
      issue_number: context.issue.number,
      owner: context.repo.owner,
      repo: context.repo.repo,
      body: linesAfterEdit.join("\r\n"),
    });
  }
};
