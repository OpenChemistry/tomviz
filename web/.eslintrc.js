module.exports = {
  extends: 'airbnb',
  rules: {
    'import/no-extraneous-dependencies': ["error", { "devDependencies": true }],
    'max-len': ["warn", 160, 4, {"ignoreUrls": true}],
    'no-multi-spaces': ["error", { exceptions: { "ImportDeclaration": true } }],
    'no-param-reassign': ["error", { props: false }],
    'no-unused-vars': ["error", { args: 'none' }],
    'react/jsx-filename-extension': ["error", { "extensions": [".js"] }],

    'no-var': 0,
    'react/prefer-es6-class': 0,
    'react/forbid-prop-types': 0,
    'jsx-a11y/no-static-element-interactions': 0,
  }
};
