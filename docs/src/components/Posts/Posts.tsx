// src/components/Posts.tsx
import React, { useEffect, useState } from 'react';
import './Posts.css';
import axios from 'axios';



interface Post {
  _id: string;
  label: string;
  date: string;
  content: string;
  media: MediaItem[];
}

interface MediaItem {
  type: string;
  src: string;
  alt: string;
}

const Posts: React.FC = () => {
  const [posts, setPosts] = useState<Post[]>([]);
  const [currentPostIndex, setCurrentPostIndex] = useState<number | null>(null);

  useEffect(() => {
    const fetchPosts = async () => {
      try {
        const response = await axios.get('http://localhost:5000/posts');
        setPosts(response.data);
      } catch (error) {
        console.error('Error fetching posts:', error);
      }
    };

    //fetchPosts();
  }, []);

  const displayLatestPosts = () => {
    const sortedPosts = [...posts].sort((a, b) => new Date(b.date).getTime() - new Date(a.date).getTime());
    return sortedPosts.map((post, index) => (
      <div key={post._id}>
        <h2>{post.label}</h2>
        <p>{post.date}</p>
        <button onClick={() => displayPostContent(index)}>Read More</button>
      </div>
    ));
  };

  const displayPostContent = (index: number) => {
    setCurrentPostIndex(index);
  };

  const displayPreviousPost = () => {
    if (currentPostIndex !== null && currentPostIndex > 0) {
      setCurrentPostIndex(currentPostIndex - 1);
    }
  };

  const displayNextPost = () => {
    if (currentPostIndex !== null && currentPostIndex < posts.length - 1) {
      setCurrentPostIndex(currentPostIndex + 1);
    }
  };

  const getCurrentPost = () => {
    if (currentPostIndex !== null) {
      const post = posts[currentPostIndex];
      const mediaContent = post.media.map((mediaItem) => {
        if (mediaItem.type === 'image') {
          return <img key={mediaItem.src} src={mediaItem.src} alt={mediaItem.alt} />;
        } else if (mediaItem.type === 'gif') {
          return <img key={mediaItem.src} src={mediaItem.src} alt={mediaItem.alt} />;
        } else if (mediaItem.type === 'video') {
          return (
            <video key={mediaItem.src} controls>
              <source src={mediaItem.src} type="video/mp4" />
              Your browser does not support the video tag.
            </video>
          );
        }
        return null;
      });

      return (
        <div>
          <h1>{post.label}</h1>
          <p>{post.date}</p>
          <p>{post.content}</p>
          {mediaContent}
          <button onClick={displayPreviousPost}>Previous Post</button>
          <button onClick={displayNextPost}>Next Post</button>
        </div>
      );
    }
    return null;
  };

  return (
<div>
      <main>
        <h2>Latest Posts</h2>
        <p>Dear users!
          Welcome to our blog. Here you can find the latest news and updates about our products and services. Enjoy reading!
          But now we are working on the implementation of the blog. Please come back later. Thank you for your understanding.
          See you soon!

          ~Majster247
        </p>

        <ul id="post-list">{displayLatestPosts()}</ul>
        <div id="post-content">{getCurrentPost()}</div>
      </main>
    </div>
  );
};

export default Posts;
