var path = require('path');
var webpack = require('webpack');
var loaders = require('./node_modules/paraviewweb/config/webpack.loaders.js');
var plugins = [];

if (process.env.NODE_ENV === 'production') {
  console.log('==> Production build');
  plugins.push(new webpack.DefinePlugin({
    'process.env': {
      NODE_ENV: JSON.stringify('production'),
    },
  }));
}

module.exports = {
  plugins: plugins,
  entry: './src/tomviz-viewer.js',
  output: {
    path: './www/tomviz',
    filename: 'tomviz.js',
    publicPath: 'tomviz/',
  },
  module: {
    preLoaders: [{
      test: /\.js$/,
      loader: 'eslint-loader',
      exclude: /node_modules/,
    }],
    loaders: [
      { test: require.resolve('./src/tomviz-viewer.js'), loader: 'expose?Tomviz' },
      { test: /\.js$/, include: /node_modules(\/|\\)arctic-viewer(\/|\\)/, loader: 'babel?presets[]=es2015,presets[]=react' },
    ].concat(loaders),
  },
  externals: {
  },
  resolve: {
    alias: {
      PVWStyle: path.resolve('./node_modules/paraviewweb/style'),
    },
  },
  postcss: [
    require('autoprefixer')({ browsers: ['last 2 versions'] }),
  ],
  eslint: {
    configFile: '.eslintrc.js',
  },
};
