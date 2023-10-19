var idx = lunr(function () {
  this.field('title')
  this.field('excerpt')
  this.field('categories')
  this.field('tags')
  this.ref('id')

  
  
    
    
      this.add({
          title: " ",
          excerpt: "Coming Soon!\n\n",
          categories: [],
          tags: [],
          id: 0
      })
      
    
      this.add({
          title: " ",
          excerpt: "WIP\n\nPaper\n\n",
          categories: [],
          tags: [],
          id: 1
      })
      
    
      this.add({
          title: " ",
          excerpt: "Portfolio Here you’ll find summaries of the major projects I’ve done over the years, which are representative of my skills...",
          categories: [],
          tags: [],
          id: 2
      })
      
    
  
    
    
      this.add({
          title: "Embedding Wasmtime in C++",
          excerpt: "I think by now we all can agree, that WebAssembly is an exciting technology WebAssembly and C++ If you’re interested...",
          categories: ["posts"],
          tags: [],
          id: 3
      })
      
    
      this.add({
          title: "Work-In-Progress",
          excerpt: "Welcome First of all, thank you for visiting my website. As you might have seen, it’s quite barren here and...",
          categories: ["posts"],
          tags: [],
          id: 4
      })
      
    
  
});

console.log( jQuery.type(idx) );

var store = [
  
    
    
    
      
      {
        "title": " ",
        "url": "http://localhost:4000/portfolio/coming-soon/",
        "excerpt": "Coming Soon!\n\n",
        "teaser":
          
            null
          
      },
    
      
      {
        "title": " ",
        "url": "http://localhost:4000/portfolio/fog_nodes/",
        "excerpt": "WIP\n\nPaper\n\n",
        "teaser":
          
            null
          
      },
    
      
      {
        "title": " ",
        "url": "http://localhost:4000/portfolio/index.html",
        "excerpt": "Portfolio Here you’ll find summaries of the major projects I’ve done over the years, which are representative of my skills...",
        "teaser":
          
            null
          
      },
    
  
    
    
    
      
      {
        "title": "Embedding Wasmtime in C++",
        "url": "http://localhost:4000/posts/2023/07/04/wasmtime-cpp.html",
        "excerpt": "I think by now we all can agree, that WebAssembly is an exciting technology WebAssembly and C++ If you’re interested...",
        "teaser":
          
            null
          
      },
    
      
      {
        "title": "Work-In-Progress",
        "url": "http://localhost:4000/posts/2023/10/01/coming-soon.html",
        "excerpt": "Welcome First of all, thank you for visiting my website. As you might have seen, it’s quite barren here and...",
        "teaser":
          
            null
          
      }
    
  ]

$(document).ready(function() {
  $('input#search').on('keyup', function () {
    var resultdiv = $('#results');
    var query = $(this).val().toLowerCase().replace(":", "");
    var result =
      idx.query(function (q) {
        query.split(lunr.tokenizer.separator).forEach(function (term) {
          q.term(term, {  boost: 100 })
          if(query.lastIndexOf(" ") != query.length-1){
            q.term(term, {  usePipeline: false, wildcard: lunr.Query.wildcard.TRAILING, boost: 10 })
          }
          if (term != ""){
            q.term(term, {  usePipeline: false, editDistance: 1, boost: 1 })
          }
        })
      });
    resultdiv.empty();
    resultdiv.prepend('<p class="results__found">'+result.length+' Result(s) found</p>');
    for (var item in result) {
      var ref = result[item].ref;
      if(store[ref].teaser){
        var searchitem =
          '<div class="list__item">'+
            '<article class="archive__item" itemscope itemtype="http://schema.org/CreativeWork">'+
              '<h2 class="archive__item-title" itemprop="headline">'+
                '<a href="'+store[ref].url+'" rel="permalink">'+store[ref].title+'</a>'+
              '</h2>'+
              '<div class="archive__item-teaser">'+
                '<img src="'+store[ref].teaser+'" alt="">'+
              '</div>'+
              '<p class="archive__item-excerpt" itemprop="description">'+store[ref].excerpt+'</p>'+
            '</article>'+
          '</div>';
      }
      else{
    	  var searchitem =
          '<div class="list__item">'+
            '<article class="archive__item" itemscope itemtype="http://schema.org/CreativeWork">'+
              '<h2 class="archive__item-title" itemprop="headline">'+
                '<a href="'+store[ref].url+'" rel="permalink">'+store[ref].title+'</a>'+
              '</h2>'+
              '<p class="archive__item-excerpt" itemprop="description">'+store[ref].excerpt+'</p>'+
            '</article>'+
          '</div>';
      }
      resultdiv.append(searchitem);
    }
  });
});
