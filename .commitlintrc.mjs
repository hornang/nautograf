const scopes = [
  'scene',
  'tilefactory',
];

export default {
  extends: ['@commitlint/config-conventional'],

  rules: {
    'header-max-length': [2, 'always', 72],
    'subject-case': [2, 'always', 'sentence-case'],
    'scope-enum': [2, 'always', scopes],
  },
};
