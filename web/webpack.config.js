var path = require('path');
var webpack = require('webpack');

var plugins = [];
var loaders = [
  {
    test: /\.svg$/,
    loader: 'svg-sprite',
    exclude: /fonts/,
  }, {
    test: /\.woff(2)?(\?v=[0-9]\.[0-9]\.[0-9])?$/,
    loader: 'url-loader?limit=600000&mimetype=application/font-woff',
  }, {
    test: /\.(ttf|eot|svg)(\?v=[0-9]\.[0-9]\.[0-9])?$/,
    loader: 'url-loader?limit=600000',
    include: /fonts/,
  }, {
    test: /\.(png|jpg)$/,
    loader: 'url-loader?limit=600000',
  }, {
    test: /\.css$/,
    loader: 'style!css!postcss',
  }, {
    test: /\.mcss$/,
    loader: 'style!css?modules&importLoaders=1&localIdentName=[name]_[local]_[hash:base64:5]!postcss',
  }, {
    test: /\.c$/i,
    loader: 'shader',
  }, {
    test: /\.json$/,
    loader: 'json-loader',
  }, {
    test: /\.html$/,
    loader: 'html-loader',
  }, {
    test: /\.glsl$/,
    loader: 'shader',
  }, {
    test: /\.js$/,
    include: /node_modules(\/|\\)paraviewweb(\/|\\)/,
    loader: 'babel?presets[]=es2015,presets[]=react',
  }, {
    test: /\.js$/,
    exclude: /node_modules/,
    loader: 'babel?presets[]=es2015,presets[]=react',
  }, {
    test: /\.js$/,
    include: /node_modules(\/|\\)arctic-viewer(\/|\\)/,
    loader: 'babel?presets[]=es2015,presets[]=react',
  },
];

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
    path: './www',
    filename: 'tomviz.js',
  },
  module: {
    preLoaders: [{
      test: /\.js$/,
      loader: 'eslint-loader',
      exclude: /node_modules/,
    }],
    loaders: loaders,
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
